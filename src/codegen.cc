#include "codegen.hh"

#include <fmt/format.h>

#include <iostream>

#include "fsm.hh"
#include "util.hh"

using fmt::format;

namespace fsm {

constexpr char INDENTATION[] = "  ";

Property::Property(uint32_t id, std::string clk_name, const fsm::Node *state_var1,
                   const fsm::Node *state_value1)
    : id(id), state_var1(state_var1), state_value1(state_value1), clk_name(std::move(clk_name)) {}

Property::Property(uint32_t id, std::string clk_name, const fsm::Node *state_var1,
                   const fsm::Node *state_value1, const fsm::Node *state_var2,
                   const fsm::Node *state_value2)
    : id(id),
      state_var1(state_var1),
      state_value1(state_value1),
      state_var2(state_var2),
      state_value2(state_value2),
      clk_name(std::move(clk_name)) {}

std::string Property::str() const {
    assert_(!clk_name.empty(), "Design does not have a clock");
    std::stringstream result;

    result << "property" << property_name() << std::endl;
    // clock
    result << INDENTATION << "@(posedge " << clk_name << ") ";
    // compute the SVA expressions
    assert_(state_var1 && state_value1, "state cannot be null");
    {
        const auto state_var_name = ::format("{0}.{1}", TOP_NAME, state_var1->handle_name());
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
        const auto state_var_name = ::format("{0}.{1}", TOP_NAME, state_var1->handle_name());
        const auto state_value = ::format("{0}", state_value1->value);
        result << state_var_name << " == " << state_value << ";";
    }
    result << std::endl;

    result << "endproperty" << std::endl;
    // cover
    result << property_label() << " cover property (" << property_name() << ");" << std::endl;

    return result.str();
}

std::string Property::property_name() const { return ::format("fsm_state_{0}", id); }

std::string Property::property_label() const { return ::format("FSM_STATE_{0}:", id); }

Module::Module(fsm::Graph *graph, const std::string &top_name) {
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
        throw std::invalid_argument("Top module not set");
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
}

void Module::create_properties() {
    // compute the coupled FSM here?
    uint32_t id_count = 0;
    for (auto const &fsm : fsm_results_) {
        // this is for reachable state
        if (fsm.is_counter()) continue;
        auto unique_states = fsm.unique_states();
        for (auto const &state : unique_states) {
            // so single variable
            auto property = std::make_shared<Property>(id_count++, clk_name_, fsm.node(), state);
            properties_.emplace(property->id, property);
        }
        // state transition
        for (auto const &state_from : unique_states) {
            for (auto const &state_to : unique_states) {
                auto property = std::make_shared<Property>(id_count++, clk_name_, fsm.node(),
                                                           state_from, fsm.node(), state_to);
                properties_.emplace(property->id, property);
            }
        }
    }
}

}  // namespace fsm
