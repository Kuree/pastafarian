#include "fsm.hh"

#include <map>
#include <utility>

#include "util.hh"

namespace fsm {

FSMResult::FSMResult(const fsm::Node *node, std::unordered_set<const Edge *> const_src)
    : node_(node), const_src_(std::move(const_src)) {
    is_counter_ = Graph::is_counter(node_, const_src_);
}

std::set<std::pair<const Node *, const Node *>> FSMResult::self_arc() const {
    std::set<std::pair<const Node *, const Node *>> result;
    // if it's a counter-based, there is still change to be a normal FSM since some people wrote
    // +1 as the next state.
    // maybe use bounded model for this one?

    // first we figure out the "unique" state. notice that even though sometimes the constant
    // may be the same value, it could be set in a different scope (true for constant values as
    // number literals; enum will be the same node since it's a named value)
    // this could be used to potentially figure out how to trigger the transition arc (if we choose
    // to retain the operator in the node)

    // this is for the flattened version of const src
    // first item is the constant, second one is where it connected. this allows us to specify
    // the arg
    std::set<std::pair<const Node *, const Node *>> edge_states;
    for (auto edge : const_src_) {
        edge_states.emplace(edge->from, edge->to);
    }

    std::set<std::pair<const Node *, const Node *>> visited;

    for (auto const [from_node, from_node_next] : edge_states) {
        for (auto const [to_node, to_node_next] : edge_states) {
            if (from_node == to_node) continue;
            if (visited.find({from_node_next, to_node_next}) != visited.end()) continue;
            // if one current state influence another
            // then from_node_next should have a route to to_node_next
            // in most cases
            // again, bmc could work for this case?
            if (Graph::reachable(from_node_next, to_node_next)) {
                result.emplace(std::make_pair(from_node, to_node));
            }
            visited.emplace(std::make_pair(from_node_next, to_node_next));
        }
    }

    return result;
}

Node *get_direct_const(const Edge *edge) {
    // TODO: handle case expressions where there are expression groups
    Node *from = edge->from;
    if (from->type == NodeType::Constant) return from;

    while (from->edges_from.size() == 1) {
        from = (*from->edges_from.begin())->from;
        if (from->type == NodeType::Constant) {
            return from;
        }
    }
    return nullptr;
};

std::set<std::pair<const Node *, const Node *>> FSMResult::syntax_arc() const {
    // notice that this is not guaranteed to be complete, but have zero false positive.

    std::set<std::pair<const Node *, const Node *>> result;
    // counter based doesn't have arc transition
    if (is_counter_) return result;

    auto cond = [](const Edge *edge) -> bool {
        if (edge->is_assign()) {
            auto node_to = edge->to;
            if (node_to->op == NetOpType::Equal) {
                // notice that we only deal with if (state == a) case
                // it's a comparison
                auto const &edges_from = node_to->edges_from;
                Node *n = nullptr;
                for (auto const edge_from : edges_from) {
                    if (!n && edge_from->is_assign()) {
                        n = get_direct_const(edge_from);
                    }
                }
                return n != nullptr;
            }
        }

        return false;
    };

    // find all the comparison nodes that compare the state variable with different
    // constants

    auto comp_edges = Graph::find_connection_cond(node_, cond);
    for (auto const node_edge : comp_edges) {
        auto node_comp = node_edge->to;
        Node *node_comp_control;
        if (node_comp->has_type(NodeType::Control)) {
            node_comp_control = node_comp;
        } else {
            assert_(node_comp->edges_to.size() == 1, "condition has 1 fan-out");
            node_comp_control = node_comp->edges_to.begin()->get()->to;
        }
        // find the assign nodes
        for (auto const &edge : const_src_) {
            auto assign_node = edge->to;
            // find out if it has false path
            Edge *false_edge = nullptr;
            for (auto const &edge_to : node_comp_control->edges_to) {
                if (edge_to->has_type(EdgeType::False)) {
                    false_edge = edge_to.get();
                    break;
                }
            }
            if (assign_node->child_of(node_comp_control)) {
                if (false_edge) {
                    if (assign_node->child_of(false_edge->to)) {
                        continue;
                    }
                }
                // this is one transition arc
                auto const_to = edge->from;
                Node *const_from = nullptr;
                for (auto const e : node_comp->edges_from) {
                    if (!const_from) {
                        const_from = get_direct_const(e);
                    }
                }

                assert_(const_from != nullptr, "Unable to find const from");
                result.emplace(std::make_pair(const_from, const_to));
            }
        }
    }

    std::set<std::pair<int64_t, int64_t>> unique_values;
    std::set<std::pair<const Node *, const Node *>> unique_result;
    for (auto const &[n1, n2] : result) {
        if (unique_values.find({n1->value, n2->value}) == unique_values.end()) {
            unique_result.emplace(std::make_pair(n1, n2));
            unique_values.emplace(std::make_pair(n1->value, n2->value));
        }
    }

    return result;
}

std::vector<const Node *> FSMResult::unique_states() const {
    std::map<int64_t, const Node *> values;
    std::vector<const Node *> result;

    for (auto const &edge : const_src_) {
        auto n = edge->from;
        auto v = n->value;
        if (values.find(v) == values.end()) {
            values.emplace(v, n);
        }
    }
    result.reserve(values.size());
    for (auto const &iter : values) result.emplace_back(iter.second);
    return result;
}

}  // namespace fsm