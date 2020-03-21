#include <CLI/CLI.hpp>
#include <iostream>

#include "fsm.hh"
#include "parser.hh"
#include "util.hh"

void print_out_fsm(const fsm::FSMResult &fsm_result) {
    std::cout << "State variable name: " << fsm_result.node()->handle_name() << std::endl;
    if (fsm_result.is_counter()) {
        std::cout << "  State: counter" << std::endl;
    } else {
        auto states = fsm_result.unique_states();
        for (auto const &state: states) {
            if (!state->name.empty()) {
                std::cout << "  State: " << state->name << " (" << state->value << ")" << std::endl;
            } else {
                std::cout << "  State: " << state->value << std::endl;
            }
        }
    }
}

void print_grouped_fsm(const std::vector<fsm::FSMResult> &fsm_results) {
    auto result = fsm::Graph::group_fsms(fsm_results);
    if (result.empty()) return;
    uint64_t count = 0;
    for (auto const &[node, linked_nodes] : result) {
        if (linked_nodes.empty()) continue;
        std::cout << node->handle_name() << ": " << std::endl;
        for (auto const linked_node: linked_nodes) {
            std::cout << "  - " << linked_node->handle_name() << std::endl;
        }

        if (count++ < result.size()) {
            std::cout << std::endl;
        }
    }
}

int main(int argc, char *argv[]) {
    CLI::App app{"FSM Detector"};
    std::vector<std::string> include_dirs;
    std::vector<std::string> filenames;
    app.add_option("-i,--input", filenames, "SystemVerilog design files");
    app.add_option("-I,--include", include_dirs, "SystemVerilog include search directory");

    CLI11_PARSE(app, argc, argv)

    if (filenames.empty()) {
        app.exit(CLI::Error("filename", "filenames cannot be empty"));
        return EXIT_FAILURE;
    }

    auto print_verilog_filenames = fsm::string::join(filenames.begin(), filenames.end(), " ");
    std::cout << "Start parsing verilog file " << print_verilog_filenames << std::endl;

    auto json_filename = fsm::parse_verilog(filenames, include_dirs);

    // parse the design
    std::cout << "Start parsing design..." << std::endl;
    fsm::Graph g;
    fsm::Parser p(&g);
    p.parse(json_filename);

    // get FSMs
    std::cout << "Detecting FSM..." << std::endl;
    auto const fsms = g.identify_fsms();
    if (fsms.empty()) {
        std::cerr << "No FSM detected" << std::endl;
        return EXIT_FAILURE;
    }
    for (uint64_t i = 0; i < fsms.size(); i++) {
        print_out_fsm(fsms[i]);
        if (i != fsms.size() - 1) {
            std::cout << std::endl;
        }
    }

    // see coupled FSM
    std::cout << "Calculating coupled FSMs..." << std::endl;
    print_grouped_fsm(fsms);
}
