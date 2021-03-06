#include "fsm.hh"

#include <cxxpool.h>

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

auto comp_terminate = [](const Edge *edge) -> bool {
    if (edge->is_assign()) {
        auto node_to = edge->to;
        if (node_to->op != NetOpType::Ignore) return true;
        if (node_to->edges_from.size() > 1) return true;
    }
    return false;
};

std::unordered_set<const Node *> FSMResult::comp_const() const {
    auto comp_edges = Graph::find_connection_cond(node_, comp_cond, comp_terminate);
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

std::set<std::pair<const Node *, const Node *>> make_unique_result(
    const std::set<std::pair<const Node *, const Node *>> &result) {
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

void FSMResult::extract_fsm_arcs() {
    // notice that this is not guaranteed to be complete, but have zero false positive.

    std::set<std::pair<const Node *, const Node *>> result;
    // counter based doesn't have arc transition
    if (is_counter_) return;

    // find all the comparison nodes that compare the state variable with different
    // constants

    auto comp_edges = Graph::find_connection_cond(node_, comp_cond, comp_terminate);
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
                    Node *const_from = get_const_from_comp(node_comp);

                    assert_(const_from != nullptr, "Unable to find const from");
                    result.emplace(std::make_pair(const_from, const_to));
                }
            }
        }
    }
    syntax_arc_ = make_unique_result(result);
}

Node *FSMResult::get_const_from_comp(const Node *node_comp) {
    Node *const_from = nullptr;
    for (auto const e : node_comp->edges_from) {
        if (!const_from) {
            const_from = get_direct_const(e);
        }
    }
    return const_from;
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

std::unordered_set<const Node *> FSMResult::counter_values() const {
    if (!is_counter_) return {};
    // need to compute counter values
    // this is the values that used explicitly test to control the circuit logic
    // such as max, etc
    // this will be used to test if these values are reachable or not
    // notice that this is only used when we have a comparison
    // hence we can re-use the comp const function here
    auto values_comp = comp_const();

    std::unordered_set<const Node *> result;
    std::unordered_set<int64_t> values;

    for (auto const node : values_comp) {
        if (values.find(node->value) == values.end()) {
            values.emplace(node->value);
            result.emplace(node);
        }
    }

    for (auto const &edge : const_src_) {
        auto to = edge->to;
        if (!to->has_type(NodeType::Assign)) continue;
        auto n = edge->from;
        auto v = n->value;
        if (values.find(v) == values.end()) {
            values.emplace(v);
            result.emplace(n);
        }
    }

    return result;
}

void FSMResult::merge_fsm(FSMResult &fsm) {
    assert_(is_counter_ == fsm.is_counter_, "");
    // notice that the count src would be the same since they are pipelined
    // however, the syntax arc won't be
    syntax_arc_.insert(fsm.syntax_arc_.begin(), fsm.syntax_arc_.end());
}

void identify_fsm_arcs(std::vector<FSMResult> &fsm_result) {
    auto num_cpus = get_num_cpus();
    cxxpool::thread_pool pool{num_cpus};
    std::vector<std::future<void>> tasks;
    tasks.reserve(fsm_result.size());

    for (auto &fsm : fsm_result) {
        auto t = pool.push([&]() -> void { fsm.extract_fsm_arcs(); });
        tasks.emplace_back(std::move(t));
    }
    for (auto &t : tasks) {
        t.wait();
    }
    for (auto &t : tasks) {
        t.get();
    }
}

bool is_pipelined(const Node *from, const Node *to) {
    auto predicate = [](const Edge *edge) -> bool {
        if (edge->has_type(EdgeType::Control)) return false;
        // only for fan out one
        return !edge->to->has_type(NodeType::Control) && !edge->to->has_type(NodeType::Net);
    };
    // usually the assignment chain won't be more than 16 nodes
    // otherwise whoever create this design is really stupid...
    auto path = Graph::route(from, to, predicate, 16);
    if (path.empty()) return false;
    // need to check if there is an non-blocking edge
    for (uint64_t i = 0; i < path.size() - 1; i++) {
        auto node_from = path[i];
        auto node_to = path[i + 1];
        // need to check the edge
        const Edge *edge = nullptr;
        for (auto &e : node_from->edges_to) {
            if (e->to == node_to) {
                edge = e.get();
                break;
            }
        }
        if (edge->has_type(EdgeType::NonBlocking) && edge->to == to) {
            return true;
        }
    }
    return false;
}

void merge_pipelined_fsm(std::vector<FSMResult> &fsm_result) {
    // this one only merges directly pipelined fsm
    std::unordered_map<FSMResult *, FSMResult *> pipelined_fsm;
    for (auto &fsm_from : fsm_result) {
        for (auto &fsm_to : fsm_result) {
            if (&fsm_from != &fsm_to && !fsm_from.is_counter() && !fsm_to.is_counter()) {
                auto from_node = fsm_from.node();
                auto to_node = fsm_to.node();
                if (is_pipelined(from_node, to_node)) {
                    pipelined_fsm.emplace(std::make_pair(&fsm_from, &fsm_to));
                }
            }
        }
    }
    // early out the computation
    if (pipelined_fsm.empty()) return;
    // need to find root FSM
    // union find
    struct Root {
        FSMResult *result;
    };
    std::unordered_map<FSMResult *, std::shared_ptr<Root>> fsm_root;
    std::unordered_map<std::shared_ptr<Root>, std::unordered_set<FSMResult *>> fsms;
    for (auto &[fsm_from, fsm_to] : pipelined_fsm) {
        if (fsm_root.find(fsm_from) != fsm_root.end()) {
            // need to merge everything
            auto root = fsm_root.at(fsm_from);
            fsms.at(root).emplace(fsm_to);
            fsm_root.emplace(fsm_to, root);
        } else if (fsm_root.find(fsm_to) != fsm_root.end()) {
            // upgrade the root
            auto root = fsm_root.at(fsm_to);
            root->result = fsm_from;
            fsms.at(root).emplace(fsm_from);
        } else {
            // new stuff
            auto root = std::make_shared<Root>();
            root->result = fsm_from;
            fsm_root.emplace(fsm_from, root);
            fsm_root.emplace(fsm_to, root);
            fsms.emplace(root, std::unordered_set<FSMResult *>{fsm_from, fsm_to});
        }
    }

    // now we need to merge them
    for (auto [root, children] : fsms) {
        auto root_fsm = root->result;
        for (auto child : children) {
            if (child != root_fsm) root_fsm->merge_fsm(*child);
        }
    }
    // delete the fsm that's been merged
    for (auto const &[root, set] : fsms) {
        for (auto child : set) {
            if (child != root->result) {
                auto pos = std::find(fsm_result.begin(), fsm_result.end(), *child);
                assert_(pos != fsm_result.end(), "cannot find fsm when merging");
                fsm_result.erase(pos);
            }
        }
    }
}

}  // namespace fsm