#include "fsm.hh"
#include "util.hh"

#include <map>
#include <utility>

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

std::set<std::pair<const Node *, const Node *>> FSMResult::syntax_arc() const {
    // this is an O(n^2) algorithm to find possible transitions from the graph
    // notice that this is not guaranteed to be complete, but have zero false positive.

    std::set<std::pair<const Node *, const Node *>> result;

    auto cond = [](const Edge *edge) -> bool {
        if (edge->type == EdgeType::NonBlocking || edge->type == EdgeType::Blocking) {
            auto node_to = edge->to;
            if (node_to->op == NetOpType::Equal && node_to->edges_from.size() == 2) {
                // it's a comparison
                auto const &edges_from = node_to->edges_from;
                Node *n = nullptr;
                for (auto const edge_from : edges_from) {
                    if (edge_from->from->type == NodeType::Constant) {
                        n = edge_from->from;
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
    // find the assign nodes
    for (auto const &edge : const_src_) {
        auto assign_node = edge->to;
        for (auto const node_edge: comp_edges) {
            auto node_comp = node_edge->to;
            if (Graph::has_path(node_comp, assign_node)) {
                // this is one transition arc
                auto const_to = edge->from;
                Node *const_from = nullptr;
                for (auto const e : node_comp->edges_from)
                    if (e->from->type == NodeType::Constant)
                        const_from = e->from;
                assert_(const_from != nullptr, "Unable to find const from");
                result.emplace(std::make_pair(const_from, const_to));
            }
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