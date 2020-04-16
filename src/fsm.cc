#include "fsm.hh"

#include <map>
#include <queue>
#include <utility>

#include "util.hh"

namespace fsm {

FSMResult::FSMResult(const fsm::Node *node, std::unordered_set<const Edge *> const_src)
    : node_(node), const_src_(std::move(const_src)) {
    is_counter_ = Graph::is_counter(node_, const_src_);
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

auto comp_cond = [](const Edge *edge) -> bool {
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

std::unordered_set<const Node *> FSMResult::comp_const() const {
    auto comp_edges = Graph::find_connection_cond(node_, comp_cond);
    std::unordered_set<const Node *> result;
    for (auto const node_edge : comp_edges) {
        auto node_comp = node_edge->to;
        Node *const_from = nullptr;
        for (auto const e : node_comp->edges_from) {
            if (!const_from) {
                const_from = get_direct_const(e);
            }
        }
        assert_(const_from != nullptr, "const is null");
        result.emplace(const_from);
    }
    // make the value unique
    std::unordered_set<const Node *> unique_result;
    std::unordered_set<int64_t> unique_values;
    for (auto const node : result) {
        if (unique_values.find(node->value) == unique_values.end()) {
            unique_values.emplace(node->value);
            unique_result.emplace(node);
        }
    }
    return unique_result;
}

std::set<std::pair<const Node *, const Node *>> FSMResult::syntax_arc() const {
    // notice that this is not guaranteed to be complete, but have zero false positive.

    std::set<std::pair<const Node *, const Node *>> result;
    // counter based doesn't have arc transition
    if (is_counter_) return result;

    // find all the comparison nodes that compare the state variable with different
    // constants

    auto comp_edges = Graph::find_connection_cond(node_, comp_cond);
    for (auto const node_edge : comp_edges) {
        auto node_comp = node_edge->to;
        std::vector<const Node *> node_comp_control_set;
        if (node_comp->has_type(NodeType::Control)) {
            node_comp_control_set = {node_comp};
        } else {
            assert_(node_comp->edges_to.size() == 1, "condition has 1 fan-out");
            auto temp_node = node_comp;
            while (temp_node->edges_to.size() == 1 &&
                   temp_node->edges_to.begin()->get()->is_assign()) {
                temp_node = temp_node->edges_to.begin()->get()->to;
            }
            // whether it is a named variable or not. if it is, it means that a named wire
            // is used as a condition, typically in Chisel
            // if not, it means we're using normal net
            if (temp_node->name.empty()) {
                node_comp_control_set = {node_comp->edges_to.begin()->get()->to};
            } else {
                std::queue<const Node *> working_set;
                std::unordered_set<const Node *> visited;
                working_set.emplace(temp_node);
                while (!working_set.empty()) {
                    auto n = working_set.front();
                    working_set.pop();
                    if (visited.find(n) != visited.end()) {
                        continue;
                    } else {
                        visited.emplace(n);
                    }
                    const static std::unordered_set<NetOpType> allowed_ops = {NetOpType::BinaryAnd,
                                                                              NetOpType::BinaryOr};
                    const static std::unordered_set<NetOpType> disallowed_ops = {
                        NetOpType::LogicalNot, NetOpType::BitwiseNot};
                    for (auto const &edge : n->edges_to) {
                        auto nn = edge->to;
                        if (nn->has_type(NodeType ::Control) &&
                            disallowed_ops.find(nn->op) == disallowed_ops.end() && nn->parent) {
                            node_comp_control_set.emplace_back(nn);
                        } else if (edge->is_assign() && !nn->has_type(NodeType::Control) &&
                                   (!nn->name.empty() ||
                                    allowed_ops.find(nn->op) != allowed_ops.end())) {  // NOLINT
                            working_set.emplace(nn);
                        } else if (edge->is_assign() && nn->edges_to.size() == 1 &&
                                   (*nn->edges_to.begin())->is_assign() &&
                                   !nn->has_type(NodeType::Control)) {
                            working_set.emplace(nn);
                        }
                    }
                }
            }
        }
        // find the assign nodes
        for (auto const &edge : const_src_) {
            auto assign_node = edge->to;
            for (auto const node_comp_control : node_comp_control_set) {
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
    }

    std::set<std::pair<int64_t, int64_t>> unique_values;
    std::set<std::pair<const Node *, const Node *>> unique_result;
    for (auto const &[n1, n2] : result) {
        if (unique_values.find({n1->value, n2->value}) == unique_values.end()) {
            unique_result.emplace(std::make_pair(n1, n2));
            unique_values.emplace(std::make_pair(n1->value, n2->value));
        }
    }

    return unique_result;
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