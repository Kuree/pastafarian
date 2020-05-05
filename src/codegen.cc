#include "codegen.hh"

#include <cxxpool.h>
#include <fmt/format.h>
#include <tqdm.h>

#include <fstream>
#include <iostream>
#include <subprocess.hpp>

#include "fsm.hh"
#include "source.hh"
#include "util.hh"

using fmt::format;

namespace fsm {

constexpr char INDENTATION[] = "  ";

Property::Property(uint32_t id, const Node *top, std::string clk_name, const fsm::Node *state_var1,
                   const fsm::Node *state_value1)
    : id(id),
      top(top),
      state_var1(state_var1),
      state_value1(state_value1),
      clk_name(std::move(clk_name)) {}

Property::Property(uint32_t id, const Node *top, std::string clk_name, const fsm::Node *state_var1,
                   const fsm::Node *state_value1, const fsm::Node *state_var2,
                   const fsm::Node *state_value2)
    : id(id),
      top(top),
      state_var1(state_var1),
      state_value1(state_value1),
      state_var2(state_var2),
      state_value2(state_value2),
      clk_name(std::move(clk_name)) {}

std::string Property::str() const {
    assert_(!clk_name.empty(), "Design does not have a clock");
    std::stringstream result;

    result << "property " << property_name() << ";" << std::endl;
    // clock
    result << INDENTATION << "@(posedge " << clk_name << ") ";
    // compute the SVA expressions
    assert_(state_var1 && state_value1, "state cannot be null");
    {
        const auto state_var_name = state_var1->handle_name(top);
        const auto state_value = ::format("{0}", state_value1->value);
        result << state_var_name << " == " << state_value;
    }
    if (state_var2 && state_value2) {
        // we need to figure out if it has second part
        std::string op;
        if (delay == 0) {
            op = "|->";
        } else {
            op = "|=>";
            if (delay > 1) {
                op = ::format("{0} ##{1}", op, delay - 1);
            }
        }
        result << " " << op << " ";
        const auto state_var_name = state_var2->handle_name(top);
        const auto state_value = ::format("{0}", state_value2->value);
        result << state_var_name << " == " << state_value;
    }
    result << ";" << std::endl;

    result << "endproperty" << std::endl;
    // cover
    result << property_label() << " cover property (" << property_name() << ");" << std::endl;

    return result.str();
}

std::string Property::property_name() const { return ::format("fsm_state_{0}", id); }

std::string Property::property_label() const {
    return ::format("{0}{1}:", PROPERTY_LABEL_PREFIX, id);
}

VerilogModule::VerilogModule(fsm::Graph *graph, SourceManager parser_result,
                             const std::string &top_name)
    : parser_result_(std::move(parser_result)) {
    // we loop into graph to see every module node and their parent is null
    std::unordered_map<std::string, const Node *> modules;
    auto const &nodes = graph->nodes();
    for (auto const &node : nodes) {
        if (node->type == NodeType::Module) {
            if (!node->parent || node->name == top_name ||
                (node->module_def && node->module_def->name == top_name)) {
                if (node->module_def) {
                    // this only works for the top one instantiated once
                    if (modules.find(node->module_def->name) == modules.end()) {
                        modules.emplace(node->module_def->name, node.get());
                    } else {
                        throw std::runtime_error(
                            top_name +
                            " has instantiated multiple times. Use instance name instead");
                    }
                } else {
                    modules.emplace(node->name, node.get());
                }
            }
        }
    }
    if (modules.size() > 1 && top_name.empty()) {
        std::cerr << "Top level modules:" << std::endl;
        for (auto const &iter : modules) {
            std::cerr << "  - " << iter.first << std::endl;
        }
        root_module_ = modules.begin()->second;
        std::cerr << "Using " << modules.begin()->first << " as top" << std::endl;
    } else if (modules.size() > 1) {
        if (modules.find(top_name) == modules.end()) {
            throw std::invalid_argument(top_name + " not found");
        }
        root_module_ = modules.at(top_name);
    } else {
        assert_(!modules.empty(), "no top module found");
        root_module_ = modules.begin()->second;
        if (root_module_->name != top_name && !top_name.empty()) {
            std::cerr << "Unable to find " << top_name << ". Use " << root_module_->name
                      << " instead" << std::endl;
        }
    }

    // compute the port signatures
    for (auto const &node : nodes) {
        if (node->parent == root_module_ && node->port_type != PortType::None) {
            // the ports we're interested in
            ports.emplace(node->name, node.get());
        }
    }

    // set the name
    this->name = root_module_->name;
}

void VerilogModule::create_properties() {
    // compute the coupled FSM here?
    uint32_t id_count = 0;
    auto num_cpus = get_num_cpus();
    cxxpool::thread_pool pool{num_cpus};
    std::vector<std::future<void>> tasks;
    tasks.reserve(fsm_results_.size());
    std::mutex mutex;
    tqdm bar;
    uint32_t count = 0;
    uint32_t num_fsm = fsm_results_.size();

    for (auto const &fsm_result : fsm_results_) {
        auto t = pool.push([this, &id_count, &fsm_result, &mutex, &count, num_fsm, &bar]() {
            mutex.lock();
            count++;
            bar.progress(count, num_fsm);
            mutex.unlock();

            // this is for reachable state
            auto const &fsm = fsm_result;
            if (fsm.is_counter()) {
                // get comp values
                auto counter_values = fsm.counter_values();
                if (counter_values.empty()) return;
                for (auto const value : counter_values) {
                    mutex.lock();
                    auto property = std::make_shared<Property>(id_count++, root_module_,
                                                               clock_name_, fsm.node(), value);
                    properties_.emplace(property->id, property);
                    mutex.unlock();
                }
            } else {
                auto unique_states = fsm.unique_states();
                for (auto const &state : unique_states) {
                    // so single variable
                    mutex.lock();
                    auto property = std::make_shared<Property>(id_count++, root_module_,
                                                               clock_name_, fsm.node(), state);
                    property->should_be_valid = true;
                    properties_.emplace(property->id, property);
                    mutex.unlock();
                }
                // state transition
                // get the absolute correct ones
                auto state_arcs = fsm.syntax_arc();
                std::set<std::pair<int64_t, int64_t>> state_arc_values;
                for (auto const &[from, to] : state_arcs)
                    state_arc_values.emplace(std::make_pair(from->value, to->value));
                for (auto const &state_from : unique_states) {
                    for (auto const &state_to : unique_states) {
                        mutex.lock();
                        auto property = std::make_shared<Property>(
                            id_count++, root_module_, clock_name_, fsm.node(), state_from,
                            fsm.node(), state_to);
                        auto state_pair = std::make_pair(state_from->value, state_to->value);
                        if (state_arc_values.find(state_pair) != state_arc_values.end())
                            property->should_be_valid = true;
                        property->delay = 1;
                        properties_.emplace(property->id, property);
                        mutex.unlock();
                    }
                }
            }
        });

        tasks.emplace_back(std::move(t));
    }

    for (auto &t : tasks) {
        t.wait();
    }
    for (auto &t : tasks) {
        t.get();
    }
}

void VerilogModule::add_cross_properties(
    const std::unordered_map<const Node *, std::unordered_set<const Node *>> &groups) {
    // find out the FSM result indexed by the node
    // build index
    std::unordered_map<const Node *, const FSMResult *> node_index;
    for (auto const &fsm : fsm_results_) {
        node_index.emplace(fsm.node(), &fsm);
    }
    // get the maximum id count
    uint32_t id_count = 0;
    for (auto const &iter : properties_) {
        auto id = iter.first;
        if (id > id_count) id_count = id;
    }
    id_count++;

    // notice that this is a pairwise, so we need to avoid generating redundant properties
    std::set<std::pair<const Node *, const Node *>> added_pairs;

    for (auto const &[node_from, coupled_states] : groups) {
        if (node_index.find(node_from) == node_index.end()) continue;
        auto fsm_from = node_index.at(node_from);
        if (fsm_from->is_counter()) continue;
        auto from_states = fsm_from->unique_states();
        for (auto const node_to : coupled_states) {
            if (node_index.find(node_to) == node_index.end()) continue;
            auto fsm_to = node_index.at(node_to);
            if (fsm_to->is_counter()) continue;
            auto to_states = fsm_to->unique_states();

            // creating properties for cross validation
            for (auto const from_state : from_states) {
                for (auto const to_state : to_states) {
                    if (added_pairs.find(std::make_pair(from_state, to_state)) != added_pairs.end())
                        continue;
                    auto property =
                        std::make_shared<Property>(id_count++, root_module_, clock_name_, node_from,
                                                   from_state, node_to, to_state);
                    properties_.emplace(property->id, property);
                    // add the pairs
                    added_pairs.emplace(std::make_pair(from_state, to_state));
                    added_pairs.emplace(std::make_pair(to_state, from_state));
                }
            }
        }
    }
}

Property &VerilogModule::get_property(uint32_t id) const {
    assert_(properties_.find(id) != properties_.end(), ::format("cannot find property {0}", id));
    return *properties_.at(id);
}

Property *VerilogModule::get_property(const Node *node, uint32_t state_value) const {
    for (auto const &iter : properties_) {
        auto const &prop = iter.second;
        if (prop->state_var1 == node && prop->state_value1->value == state_value &&
            !prop->state_var2) {
            return prop.get();
        }
    }
    return nullptr;
}

Property *VerilogModule::get_property(const Node *node, uint32_t state_from,
                                      uint32_t state_to) const {
    for (auto const &iter : properties_) {
        auto const &prop = iter.second;
        if (prop->state_var1 == node && prop->state_value1->value == state_from &&
            prop->state_var2 == node && prop->state_value2->value == state_to) {
            return prop.get();
        }
    }
    return nullptr;
}

std::vector<const Property *> VerilogModule::get_property(const Node *node) const {
    std::vector<const Property *> result;

    for (auto const &iter : properties_) {
        auto const &prop = iter.second;
        if (prop->state_var1 == node) {
            result.emplace_back(prop.get());
        }
    }
    assert_(!result.empty(), "no FSM states found");
    return result;
}

bool VerilogModule::has_property(const Node *node) const {
    for (auto const &iter : properties_) {
        auto const &prop = iter.second;
        if (prop->state_var1 == node) {
            return true;
        }
    }
    return false;
}

std::vector<const Property *> VerilogModule::get_property(const Node *node1,
                                                          const Node *node2) const {
    std::vector<const Property *> result;

    for (auto const &iter : properties_) {
        auto const &prop = iter.second;
        if (prop->state_var1 == node1 && prop->state_var2 == node2) {
            result.emplace_back(prop.get());
        }
    }
    assert_(!result.empty(), "no FSM states found");
    return result;
}

void VerilogModule::analyze_pins() {
    // this applies a series of heuristics to figure out the reset and clock pin name
    const static std::unordered_set<std::string> reset_names = {"rst", "rst_n", "reset", "resetn",
                                                                "reset_in"};
    const static std::unordered_set<std::string> neg_reset_names = {"rst_n", "resetn"};
    const static std::unordered_set<std::string> clock_names = {"clk", "clock", "clk_in"};
    if (clock_name_.empty()) {
        // we assume only one clock domain
        for (auto const &iter : ports) {
            if (clock_names.find(iter.first) != clock_names.end()) {
                clock_name_ = iter.first;
                break;
            }
        }
    } else {
        assert_(ports.find(clock_name_) != ports.end(), "Unable to find " + clock_name_);
    }
    if (reset_name_.empty()) {
        for (auto const &iter : ports) {
            if (reset_names.find(iter.first) != reset_names.end()) {
                reset_name_ = iter.first;
                break;
            }
        }
    } else {
        assert_(ports.find(reset_name_) != ports.end(), "Unable to find " + clock_name_);
    }
    if (neg_reset_names.find(reset_name_) != neg_reset_names.end() &&
        reset_type_ == ResetType::Default) {
        reset_type_ = ResetType::Negedge;
    }

    analyze_reset();
}

void VerilogModule::analyze_reset() {
    // skip if it already has value
    if (reset_type_ == ResetType::None) return;
    assert_(!reset_name_.empty(), "reset pin is empty");
    if (ports.find(reset_name_) != ports.end()) {
        auto reset = ports.at(reset_name_);
        // loop through the connection to see the reset type
        // this is a graph search algorithm
        auto sinks = Graph::find_sinks(reset);
        bool found = false;
        for (auto const node : sinks) {
            if (node->event_type != EventType::None) {
                // TODO: need to make sure that it's not inverted
                reset_type_ = node->event_type == EventType::Posedge ? ResetType::Posedge
                                                                     : ResetType::Negedge;
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "Unable to find reset type. Using default posedge reset" << std::endl;
        }
    } else {
        std::cerr << "reset pin (" << reset_name_ << ") not found in top" << std::endl;
    }
}

std::string VerilogModule::str() const {
    std::stringstream result;

    // module header. we create ports using the same name so it's basically a pass through
    result << "module " << TOP_NAME << "(" << std::endl;
    uint32_t count = 0;
    for (auto const &[port_name, port_node] : ports) {
        assert_(port_node->port_type != PortType::None, "Port doesn't have a direction");
        assert_(!port_node->wire_type.empty(), "Port type empty");
        result << INDENTATION << (port_node->port_type == PortType::Input ? "input" : "output")
               << " " << port_node->wire_type << " " << port_name;
        if ((++count) != ports.size()) result << ",";
        result << std::endl;
    }

    result << ");" << std::endl << std::endl;

    // dut instantiation
    assert_(root_module_->module_def != nullptr, "root module doesn't have definition");
    result << root_module_->module_def->name;
    // parameters
    if (!root_module_->module_def->params.empty()) {
        result << " #(" << std::endl << "    ";
        count = 0;
        auto const &params = root_module_->module_def->params;
        for (auto const &[param_name, param_node] : params) {
            int64_t value = param_values_.find(param_name) != param_values_.end()
                                ? param_values_.at(param_name)
                                : param_node->value;
            result << "." << param_name << "(" << value << ")";
            if (++count != params.size()) result << "," << std::endl << "    ";
        }
        result << ")";
    }
    result << " " << name << " (.*);" << std::endl << std::endl;

    // all the properties
    for (auto const &iter : properties_) {
        // skip the counter one
        auto const &prop = iter.second;
        result << prop->str() << std::endl;
    }

    // end
    result << "endmodule" << std::endl;

    return result.str();
}

void VerilogModule::to_file(const std::string &filename) const {
    std::ofstream f(filename);
    f << str();
}

void VerilogModule::set_param_values(const std::unordered_map<std::string, int64_t> &params) {
    param_values_ = params;
}

void FormalGeneration::run() {
    run_process();
    parse_result();
}

void JasperGoldGeneration::create_command_file(const std::string &cmd_filename,
                                               const std::string &wrapper_filename) {
    std::ofstream stream(cmd_filename);
    auto const &parser_result = module_.parser_result();
    auto const &files = parser_result.src_filenames();
    auto const &include_dirs = parser_result.src_include_dirs();

    // create wrapper file
    module_.to_file(wrapper_filename);

    // output the read command
    stream << "analyze -sv " << string::join(files.begin(), files.end(), " ");
    stream << " " << wrapper_filename;
    if (!include_dirs.empty()) {
        stream << " +incdir " << string::join(include_dirs.begin(), include_dirs.end(), " ");
    }
    stream << ";" << std::endl;

    // output top
    stream << "elaborate -top " << TOP_NAME << ";" << std::endl;

    // clock
    assert_(!module_.clock_name().empty(), "clock name cannot be empty");
    std::string clock_type = module_.double_edge_clock()? "-both_edges " : "";
    stream << "clock " << clock_type << module_.clock_name() << ";" << std::endl;

    // reset
    assert_(!module_.reset_name().empty(), "reset name cannot be empty");
    stream << "reset -expression ";
    // notice that this is some forms of heretics
    auto reset = module_.reset_type();
    assert_(reset != ResetType::Default,
            "reset type cannot be default. need to set reset type first");
    if (reset != ResetType::None) {
        if (reset == ResetType::Posedge) {
            stream << module_.reset_name() << ";" << std::endl;
        } else {
            stream << "~" << module_.reset_name() << ";" << std::endl;
        }
    }

    // set time limit if possible
    if (timeout_limit_) {
        stream << "set_prove_per_property_max_time_limit " << timeout_limit_ << "s;" << std::endl;
    }

    // prove them all
    stream << "prove -task {<embedded>};" << std::endl;

    // quit
    stream << "exit -force;" << std::endl;
}

constexpr char JASPERGOLD_COMMAND[] = "jaspergold";

void JasperGoldGeneration::run_process() {
    // check if jasper gold is in the shell
    std::string jg_command = fs::which(JASPERGOLD_COMMAND);
    assert_(!jg_command.empty(), "jaspergold not found in $PATH");
    // output a command file
    auto temp_dir = fs::temp_directory_path();
    auto script_filename = fs::join(temp_dir, "fsm_jg.tcl");
    auto wrapper_filename = fs::join(temp_dir, "fsm_wrapper.sv");
    create_command_file(script_filename, wrapper_filename);
    // run the script file
    auto wd = jg_working_dir();
    if (fs::exists(wd)) {
        fs::remove(wd);
    }
    auto command = ::format("{0} -allow_unsupported_OS -no_gui -proj {1} {2}", JASPERGOLD_COMMAND,
                            wd, script_filename);
    subprocess::call(command);
}

std::string JasperGoldGeneration::jg_working_dir() {
    auto temp_dir = fs::temp_directory_path();
    auto wd = fs::join(temp_dir, "fsm_jg");
    return wd;
}

bool JasperGoldGeneration::has_tools() const { return has_jaspergold(); }

bool JasperGoldGeneration::has_jaspergold() {
    std::string jg_command = fs::which(JASPERGOLD_COMMAND);
    return !jg_command.empty();
}

void JasperGoldGeneration::parse_result(const std::string &log_file) {
    // keywords
    auto const keyword = ::format("The cover property \"{0}.{1}", TOP_NAME, PROPERTY_LABEL_PREFIX);
    // parse the log
    std::ifstream file(log_file);
    for (std::string line; std::getline(file, line);) {
        // simple scanning
        auto pos = line.find(keyword);
        if (pos != std::string::npos) {
            pos += keyword.size();
            auto end_str = line.substr(pos);
            auto end = end_str.find('\"');
            if (end != std::string::npos) {
                auto id_str = line.substr(pos, end);
                uint32_t id = 0;
                try {
                    id = std::stoul(id_str);
                } catch (...) {
                }
                auto &property = module_.get_property(id);
                property.valid = line.find("unreachable") == std::string::npos;
            }
        }
    }
}

void JasperGoldGeneration::parse_result() {
    auto wd = jg_working_dir();
    auto log_dir = fs::join(fs::join(wd, "sessionLogs"), "session_0");
    auto log_file = fs::join(log_dir, "jg_session_0.log");
    assert_(fs::exists(log_file), log_file + " does not exist");
    parse_result(log_file);
}

}  // namespace fsm
