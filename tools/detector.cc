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
    const std::vector<const fsm::Property *> &properties) {
    std::vector<std::pair<const fsm::Property *, std::vector<const fsm::Property *>>> result;
    std::map<const fsm::Node *, std::vector<const fsm::Property *>> entries;
    std::map<const fsm::Node *, const fsm::Property *> states;

    for (auto const property : properties) {
        if (property->state_var2) {
            fsm::assert_(property->state_var2 == property->state_var1, "only single FSM supported");
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
            return left->state_value2 > right->state_value2;
        });
        result.emplace_back(std::make_pair(p, ps));
    }

    // sort the result as well
    std::sort(result.begin(), result.end(), [](auto const &left, auto const &right) {
        return left.first->state_value1 > right.first->state_value1;
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
    std::cout << "State variable name: " << state_node->handle_name() << std::endl;
    if (fsm_result.is_counter()) {
        std::cout << "  State: counter" << std::endl;
    } else {
        auto properties = m.get_property(state_node);
        auto sorted_result = sort_fsm_result(properties);
        for (auto const &[state_p, ps] : sorted_result) {
            auto state = state_p->state_value1;
            std::cout << "  State: ";
            print_fsm_value(state);
            if (!state_p->valid && formal) {
                std::cout << " [UNREACHABLE]";
            }
            std::cout << ":" << std::endl;
            if (!formal) continue;
            for (auto const &next_state : ps) {
                std::cout << "    - Next: ";
                print_fsm_value(next_state->state_value2);
                std::cout << std::endl;
            }
        }
    }
}

void print_grouped_fsm(
    const std::unordered_map<const fsm::Node *, std::unordered_set<const fsm::Node *>> &result) {
    if (result.empty()) return;
    uint64_t count = 0;
    for (auto const &[node, linked_nodes] : result) {
        if (linked_nodes.empty()) continue;
        std::cout << node->handle_name() << ": " << std::endl;
        for (auto const linked_node : linked_nodes) {
            std::cout << "  - " << linked_node->handle_name() << std::endl;
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
    app.add_option("-i,--input", filenames, "SystemVerilog design files");
    app.add_option("-I,--include", include_dirs, "SystemVerilog include search directory");
    app.add_option("--json", output_filename, "Output JSON. Use - for stdout");
    app.add_flag("--coupled-fsm", compute_coupled_fsm, "Whether to compute coupled FSM");
    app.add_flag("--formal", use_formal, "Whether to use formal tools to determine FSM properties");
    app.add_option("--top", top, "Specify the design top");
    app.add_option("--reset", reset_name, "Reset pin name");
    app.add_option("--clock", clock_name, "Clock pin name");

    CLI11_PARSE(app, argc, argv)

    if (filenames.empty()) {
        app.exit(CLI::Error("filename", "filenames cannot be empty"));
        return EXIT_FAILURE;
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

    // get FSMs
    std::cout << "Detecting FSM..." << std::endl;
    time_start = std::chrono::steady_clock::now();

    auto const fsms = g.identify_fsms();
    time_end = std::chrono::steady_clock::now();
    time_used = time_end - time_start;
    std::cout << "FSM detection took " << time_used.count() << " seconds" << std::endl;

    fsm::VerilogModule m(&g, manager, top);
    m.set_fsm_result(fsms);
    if (!clock_name.empty()) m.set_clock_name(clock_name);
    if (!reset_name.empty()) m.set_reset_name(reset_name);
    m.analyze_pins();
    m.create_properties();

    if (fsms.empty()) {
        std::cerr << "No FSM detected" << std::endl;
        return EXIT_FAILURE;
    }

    // set properties
    if (use_formal) {
        fsm::JasperGoldGeneration jg(m);
        jg.run();
    }

    for (uint64_t i = 0; i < fsms.size(); i++) {
        print_out_fsm(fsms[i], m, use_formal);
        if (i != fsms.size() - 1) {
            std::cout << std::endl;
        }
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

        print_grouped_fsm(fsm_groups);
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
