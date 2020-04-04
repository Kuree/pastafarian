#include "fsm.hh"

#include <utility>
#include <map>

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

std::vector<const Node *> FSMResult::unique_states() const {
    std::map<int64_t, const Node *> values;
    std::vector<const Node *> result;

    for (auto const &edge: const_src_) {
        auto n = edge->from;
        auto v = n->value;
        if (values.find(v) == values.end()) {
            values.emplace(v, n);
        }
    }
    result.reserve(values.size());
    for (auto const &iter: values)
        result.emplace_back(iter.second);
    return result;
}

}  // namespace fsm