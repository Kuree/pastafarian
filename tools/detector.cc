#include <CLI/CLI.hpp>
#include <chrono>
#include <filesystem>
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
        for (auto const &state : states) {
            if (!state->name.empty()) {
                std::cout << "  State: " << state->name << " (" << state->value << ")" << std::endl;
            } else {
                std::cout << "  State: " << state->value << std::endl;
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
    app.add_option("-i,--input", filenames, "SystemVerilog design files");
    app.add_option("-I,--include", include_dirs, "SystemVerilog include search directory");
    app.add_option("--json", output_filename, "Output JSON. Use - for stdout");
    app.add_flag("--coupled-fsm", compute_coupled_fsm, "Whether to compute coupled FSM");

    CLI11_PARSE(app, argc, argv)

    if (filenames.empty()) {
        app.exit(CLI::Error("filename", "filenames cannot be empty"));
        return EXIT_FAILURE;
    }

    auto print_verilog_filenames = fsm::string::join(filenames.begin(), filenames.end(), " ");
    std::cout << "Start parsing verilog file " << print_verilog_filenames << std::endl;

    std::string json_filename;
    auto time_start = std::chrono::steady_clock::now();
    if (filenames.size() == 1 && std::filesystem::path(filenames[0]).extension() == ".json") {
        // if it is JSON, we don't need to convert to JSON.
        json_filename = filenames[0];
    } else {
        json_filename = fsm::parse_verilog(filenames, include_dirs);
    }

    // parse the design
    std::cout << "Start parsing design..." << std::endl;
    fsm::Graph g;
    fsm::Parser p(&g);
    p.parse(json_filename);

    auto time_end = std::chrono::steady_clock::now();
    std::chrono::duration<float> time_used = time_end - time_start;
    std::cout << "Parsing took " << time_used.count() << " seconds" << std::endl;

    // get FSMs
    std::cout << "Detecting FSM..." << std::endl;
    time_start = std::chrono::steady_clock::now();
    ;
    auto const fsms = g.identify_fsms();
    time_end = std::chrono::steady_clock::now();
    time_used = time_end - time_start;
    std::cout << "FSM detection took " << time_used.count() << " seconds" << std::endl;

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
