#include "codegen.hh"

#include <fmt/format.h>

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
        const auto state_var_name = state_var1->handle_name(top);
        const auto state_value = ::format("{0}", state_value1->value);
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
            if (!node->parent || node->name == top_name) {
                modules.emplace(node->name, node.get());
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
    for (uint32_t i = 0; i < fsm_results_.size(); i++) {
        // this is for reachable state
        auto const &fsm = fsm_results_[i];
        if (fsm.is_counter()) {
            auto property = std::make_shared<Property>(id_count++, root_module_, clock_name_,
                                                       fsm.node(), nullptr);
            properties_.emplace(property->id, property);
            property_id_to_fsm_.emplace(property->id, i);
            continue;
        } else {
            auto unique_states = fsm.unique_states();
            for (auto const &state : unique_states) {
                // so single variable
                auto property = std::make_shared<Property>(id_count++, root_module_, clock_name_,
                                                           fsm.node(), state);
                properties_.emplace(property->id, property);
                property_id_to_fsm_.emplace(property->id, i);
            }
            // state transition
            for (auto const &state_from : unique_states) {
                for (auto const &state_to : unique_states) {
                    auto property =
                        std::make_shared<Property>(id_count++, root_module_, clock_name_,
                                                   fsm.node(), state_from, fsm.node(), state_to);
                    property->delay = 1;
                    properties_.emplace(property->id, property);
                    property_id_to_fsm_.emplace(property->id, i);
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
    const static std::unordered_set<std::string> reset_names = {"rst", "rst_n", "reset", "resetn"};
    const static std::unordered_set<std::string> neg_reset_names = {"rst_n", "resetn"};
    const static std::unordered_set<std::string> clock_names = {"clk", "clock"};
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
    if (neg_reset_names.find(reset_name_) != neg_reset_names.end() && !posedge_reset_) {
        posedge_reset_ = false;
    }

    analyze_reset();
}

void VerilogModule::analyze_reset() {
    assert_(!reset_name_.empty(), "reset pin is empty");
    // skip if it already has value
    if (posedge_reset_.has_value()) return;
    if (ports.find(reset_name_) != ports.end()) {
        auto reset = ports.at(reset_name_);
        // loop through the connection to see the reset type
        // this is a graph search algorithm
        auto sinks = Graph::find_sinks(reset);
        bool found = false;
        for (auto const node : sinks) {
            if (node->event_type != EventType::None) {
                // TODO: need to make sure that it's not inverted
                posedge_reset_ = node->event_type == EventType::Posedge;
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
    result << name << " " << name << " (.*);" << std::endl << std::endl;

    // all the properties
    for (auto const &iter : properties_) {
        // skip the counter one
        auto const &prop = iter.second;
        auto const &fsm = fsm_results_[property_id_to_fsm_.at(prop->id)];
        if (fsm.is_counter()) continue;
        result << prop->str() << std::endl;
    }

    // end
    result << "endmodule" << std::endl;

    return result.str();
}

void VerilogModule::to_file(const std::string &filename) {
    std::ofstream f(filename);
    f << str();
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
    stream << "clock " << module_.clock_name() << ";" << std::endl;

    // reset
    assert_(!module_.reset_name().empty(), "reset name cannot be empty");
    stream << "reset -expression ";
    // notice that this is some forms of heretics
    if (module_.posedge_reset()) {
        stream << module_.reset_name() << ";" << std::endl;
    } else {
        stream << "~" << module_.reset_name() << ";" << std::endl;
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
