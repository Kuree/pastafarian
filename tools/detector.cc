#include <CLI/CLI.hpp>
#include <chrono>
#include <codegen.hh>
#include <filesystem>
#include <iostream>

#include "fsm.hh"
#include "parser.hh"
#include "source.hh"
#include "util.hh"

std::vector<std::pair<const fsm::Property *, std::vector<const fsm::Property *>>> sort_fsm_result(
    const fsm::FSMResult &fsm_result, const std::vector<const fsm::Property *> &properties) {
    std::vector<std::pair<const fsm::Property *, std::vector<const fsm::Property *>>> result;
    std::map<const fsm::Node *, std::vector<const fsm::Property *>> entries;
    std::map<const fsm::Node *, const fsm::Property *> states;

    if (fsm_result.is_counter()) {
        for (auto const property : properties) {
            result.emplace_back(std::make_pair(property, std::vector<const fsm::Property *>{}));
        }
    } else {
        for (auto const property : properties) {
            if (property->state_var2) {
                // only print out single state
                if (property->state_var1 == property->state_var2)
                    entries[property->state_value1].emplace_back(property);
            } else {
                states[property->state_value1] = property;
            }
        }
        // merge result
        result.reserve(entries.size());
        for (auto &[node, ps] : entries) {
            auto p = states.at(node);
            // sort the properties
            std::sort(ps.begin(), ps.end(), [](auto const &left, auto const &right) {
                return left->state_value2 < right->state_value2;
            });
            result.emplace_back(std::make_pair(p, ps));
        }
    }

    // sort the result as well
    std::sort(result.begin(), result.end(), [](auto const &left, auto const &right) {
        return left.first->state_value1 < right.first->state_value1;
    });

    return result;
}

void print_fsm_value(const fsm::Node *node) {
    if (!node->name.empty()) {
        std::cout << node->name << " (" << node->value << ")";
    } else {
        std::cout << node->value;
    }
}

void print_out_fsm(const fsm::FSMResult &fsm_result, const fsm::VerilogModule &m, bool formal) {
    auto state_node = fsm_result.node();
    std::cout << "State variable name: " << state_node->handle_name(m.top()) << std::endl;
    std::string fsm_label;
    if (fsm_result.is_counter()) {
        std::cout << "  State: counter" << std::endl;
        fsm_label = "Value";
    } else {
        fsm_label = "State";
    }
    if (!m.has_property(state_node)) return;
    auto properties = m.get_property(state_node);
    auto sorted_result = sort_fsm_result(fsm_result, properties);
    for (auto const &[state_p, ps] : sorted_result) {
        auto state = state_p->state_value1;
        std::cout << "  " << fsm_label << ": ";
        print_fsm_value(state);
        if (!state_p->valid && formal) {
            std::cout << " [UNREACHABLE]";
        }
        std::cout << ":" << std::endl;
        if (formal) {
            for (auto const &next_state : ps) {
                bool should_print_next = next_state->should_be_valid | next_state->valid;
                if (should_print_next) {
                    std::cout << "    - Next: ";
                    print_fsm_value(next_state->state_value2);
                    if (!next_state->valid) {
                        std::cout << " [UNREACHABLE]";
                    }
                    std::cout << std::endl;
                }
            }
        } else {
            for (auto const &next_state : ps) {
                if (next_state->should_be_valid) {
                    std::cout << "    - Next: ";
                    print_fsm_value(next_state->state_value2);
                    std::cout << std::endl;
                }
            }
        }
    }
}

void print_grouped_fsm_formal(const fsm::FSMResult &fsm_result, const fsm::VerilogModule &m) {
    std::vector<const fsm::Property *> cross_properties;
    auto const state_from = fsm_result.node();
    auto const properties = m.get_property(state_from);
    for (auto const &prop : properties) {
        if (prop->state_var2 != state_from && prop->state_var2) {
            if (prop->valid) cross_properties.emplace_back(prop);
        }
    }

    if (!cross_properties.empty()) {
        std::cout << std::endl;
    }

    for (auto const prop : cross_properties) {
        std::cout << "- " << prop->state_var1->handle_name() << ": ";
        print_fsm_value(prop->state_value1);

        std::cout << " <==> " << prop->state_var2->handle_name() << ": ";
        print_fsm_value(prop->state_value2);

        std::cout << std::endl;
    }
}

void print_grouped_fsm(
    const fsm::VerilogModule &m,
    const std::unordered_map<const fsm::Node *, std::unordered_set<const fsm::Node *>> &result) {
    if (result.empty())
        return;
    else
        std::cout << std::endl;
    uint64_t count = 0;
    for (auto const &[node, linked_nodes] : result) {
        if (linked_nodes.empty()) continue;
        std::cout << node->handle_name(m.top()) << ": " << std::endl;
        // sort linked nodes
        auto nodes = std::vector<const fsm::Node *>(linked_nodes.begin(), linked_nodes.end());
        std::sort(nodes.begin(), nodes.end(), [=](const auto a, const auto b) {
            return a->handle_name(m.top()) < b->handle_name(m.top());
        });
        for (auto const linked_node : nodes) {
            std::cout << "  - " << linked_node->handle_name(m.top()) << std::endl;
        }

        if (count++ < result.size()) {
            std::cout << std::endl;
        }
    }
}

std::string output_json(
    const std::vector<fsm::FSMResult> &fsms,
    const std::unordered_map<const fsm::Node *, std::unordered_set<const fsm::Node *>>
        &fsm_groups) {
    fsm::json::JSONWriter w;
    w.start_array();
    for (auto const &fsm : fsms) {
        auto const node = fsm.node();
        w.start_object();
        w.write("name", node->handle_name());

        // states
        w.start_array("states");
        for (auto const &state : fsm.const_src()) {
            w.start_object();
            auto state_node = state->from;
            w.write("value", state_node->value).write("name", state_node->name);
            w.end_object();
        }
        w.end_array();

        // coupled FSM
        w.start_array("linked");
        if (fsm_groups.find(node) != fsm_groups.end()) {
            auto const &groups = fsm_groups.at(node);
            for (auto const &linked_node : groups) {
                w.write(linked_node->handle_name());
            }
        }
        w.end_array();

        w.end_object();
    }

    w.end_array();

    return w.str();
}

std::unordered_map<std::string, int64_t> get_token_values(const std::vector<std::string> &values,
                                                          bool allow_empty = false) {
    std::unordered_map<std::string, int64_t> result;
    for (auto const &value : values) {
        auto tokens = fsm::string::get_tokens(value, "=");
        if (tokens.size() == 2) {
            auto const &param_name = tokens[0];
            auto const &val_str = tokens[1];
            try {
                int64_t v = std::stoll(val_str);
                result.emplace(param_name, v);
            } catch (...) {
            }
        } else if (tokens.size() == 1 && allow_empty) {
            auto const param_name = tokens[0];
            result.emplace(param_name, 1);
        }
    }

    return result;
}

int main(int argc, char *argv[]) {
    CLI::App app{"FSM Detector"};
    std::vector<std::string> include_dirs;
    std::vector<std::string> filenames;
    std::string output_filename;
    bool compute_coupled_fsm = false;
    bool use_formal = false;
    std::string top;
    std::string clock_name;
    std::string reset_name;
    std::vector<std::string> param_values;
    std::vector<std::string> macro_values;
    std::optional<uint32_t> num_cpu;
    bool double_edge_clk = false;
    std::optional<uint32_t> property_time_limit;

    fsm::ResetType reset_type = fsm::ResetType::Default;
    std::map<std::string, fsm::ResetType> reset_type_map{
        {"none", fsm::ResetType::None},       {"default", fsm::ResetType::Default},
        {"high", fsm::ResetType::Posedge},    {"pos", fsm::ResetType::Posedge},
        {"posedge", fsm::ResetType::Posedge}, {"low", fsm::ResetType::Negedge},
        {"neg", fsm::ResetType::Negedge},     {"negedge", fsm::ResetType::Negedge}};

    app.add_option("-i,--input", filenames, "SystemVerilog design files")->required();
    app.add_option("-I,--include", include_dirs, "SystemVerilog include search directory");
    app.add_option("--json", output_filename, "Output JSON. Use - for stdout");
    app.add_flag("-c,--coupled-fsm", compute_coupled_fsm, "Whether to compute coupled FSM");
    app.add_flag("--formal", use_formal, "Whether to use formal tools to determine FSM properties");
    app.add_option("--top", top, "Specify the design top");
    app.add_option("-r,--reset,--reset-name", reset_name, "Reset pin name");
    app.add_option("--clock", clock_name, "Clock pin name");
    app.add_option("-P", param_values, "Override top port parameters");
    app.add_option("-D", macro_values, "Define preprocessor macros");
    app.add_option("-n,--num-cpu", num_cpu, "Number of CPU. Set to 0 to use all available cores");
    app.add_option("-R,--reset-type", reset_type, "Reset type")
        ->transform(CLI::CheckedTransformer(reset_type_map, CLI::ignore_case));
    app.add_flag("--double-edge-clock", double_edge_clk,
                 "Set if the design had double-edge triggered clock");
    app.add_option("-t,--time-limit", property_time_limit, "Time limit per property");

    CLI11_PARSE(app, argc, argv)

    // multi-threading support
    if (num_cpu) {
        uint32_t cpu = *num_cpu;
        fsm::set_num_cpus(cpu);
    }

    auto print_verilog_filenames = fsm::string::join(filenames.begin(), filenames.end(), " ");
    std::cout << "Start parsing verilog file " << print_verilog_filenames << std::endl;

    fsm::SourceManager manager;
    auto time_start = std::chrono::steady_clock::now();
    if (filenames.size() == 1 && std::filesystem::path(filenames[0]).extension() == ".json") {
        // if it is JSON, we don't need to convert to JSON.
        manager.set_json_filename(filenames[0]);
    } else {
        manager = fsm::SourceManager(filenames, include_dirs);
        auto macros = get_token_values(macro_values, true);
        manager.set_macros(macros);
        fsm::parse_verilog(manager);
    }

    // parse the design
    std::cout << "Start parsing design..." << std::endl;
    fsm::Graph g;
    fsm::Parser p(&g);
    p.parse(manager);

    auto time_end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time_used = time_end - time_start;
    std::cout << "Parsing took " << time_used.count() << " seconds" << std::endl;

    // top module
    fsm::VerilogModule m(&g, manager, top);

    // assign reset types
    m.set_reset_type(reset_type);
    // set double edge clock type
    m.set_double_edge_clock(double_edge_clk);

    // get FSMs
    std::cout << "Detecting FSM..." << std::endl;
    time_start = std::chrono::steady_clock::now();

    auto fsms = g.identify_fsms(m.top());

    time_end = std::chrono::steady_clock::now();
    time_used = time_end - time_start;
    uint64_t num_counters = 0;
    for (auto const &fsm_result : fsms)
        if (fsm_result.is_counter()) num_counters++;
    std::cout << "FSM detection took " << time_used.count() << " seconds. "
              << fsms.size() - num_counters << " FSMs " << num_counters << " counters" << std::endl;

    std::cout << "Analyzing detected FSM..." << std::endl;
    time_start = std::chrono::steady_clock::now();

    // extract syntax arc
    fsm::identify_fsm_arcs(fsms);

    m.set_fsm_result(fsms);
    if (!clock_name.empty()) m.set_clock_name(clock_name);
    if (!reset_name.empty()) m.set_reset_name(reset_name);
    m.analyze_pins();
    m.create_properties();

    time_end = std::chrono::steady_clock::now();
    time_used = time_end - time_start;
    std::cout << "FSM analysis took " << time_used.count() << " seconds" << std::endl;

    if (fsms.empty()) {
        std::cerr << "No FSM detected" << std::endl;
        return EXIT_FAILURE;
    }

    // see coupled FSM
    std::unordered_map<const fsm::Node *, std::unordered_set<const fsm::Node *>> fsm_groups;
    if (compute_coupled_fsm) {
        std::cout << "Calculating coupled FSMs..." << std::endl;
        time_start = std::chrono::steady_clock::now();
        fsm_groups = fsm::Graph::group_fsms(fsms);
        time_end = std::chrono::steady_clock::now();
        time_used = time_end - time_start;
        std::cout << "FSM coupling took " << time_used.count() << " seconds" << std::endl;

        // generate cross property coverage
        m.add_cross_properties(fsm_groups);
    }

    // set properties
    if (use_formal) {
        fsm::JasperGoldGeneration jg(m);
        auto parameters = get_token_values(param_values);
        m.set_param_values(parameters);
        if (property_time_limit) {
            jg.set_timeout_limit(*property_time_limit);
        }
        jg.run();
    }

    for (uint64_t i = 0; i < fsms.size(); i++) {
        print_out_fsm(fsms[i], m, use_formal);
        if (i != fsms.size() - 1) {
            std::cout << std::endl;
        }
    }

    if (compute_coupled_fsm) {
        if (use_formal) {
            for (uint64_t i = 0; i < fsms.size(); i++) {
                print_grouped_fsm_formal(fsms[i], m);
                if (i != fsms.size() - 1) {
                    std::cout << std::endl;
                }
            }
        } else {
            print_grouped_fsm(m, fsm_groups);
        }
    }

    if (!output_filename.empty()) {
        auto str = output_json(fsms, fsm_groups);
        if (output_filename == "-") {
            std::cout << str << std::endl;
        } else {
            std::ofstream output(output_filename);
            output << str;
        }
    }
}
