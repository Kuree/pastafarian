#include "graph.hh"

#include <cxxpool.h>
#include <tqdm.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <queue>
#include <stack>

#include "fsm.hh"
#include "util.hh"

namespace fsm {

std::string Node::handle_name() const { return handle_name(nullptr); }

std::string Node::handle_name(const Node *top) const {
    std::stack<std::string> names;
    auto node = this;
    while (node) {
        assert_(!node->name.empty(), "node name empty");
        names.emplace(node->name);
        if (node == top) break;
        node = node->parent;
    }
    std::vector<std::string> reorder_names;
    reorder_names.reserve(names.size());
    while (!names.empty()) {
        reorder_names.emplace_back(names.top());
        names.pop();
    }
    return string::join(reorder_names.begin(), reorder_names.end(), ".");
}

bool Node::child_of(const Node *node) const {
    if (!node) return false;
    auto p = parent;
    while (p) {
        if (p != node) {
            p = p->parent;
        } else {
            return true;
        }
    }
    return false;
}

Node *Graph::get_node(uint64_t key) {
    if (has_node(key)) {
        return nodes_map_.at(key);
    } else {
        // create an empty node
        auto node = add_node(key, "");
        return node;
    }
}

bool Graph::has_path(Node *from, Node *to, uint64_t max_depth) {
    // DFS based search
    std::stack<Node *> nodes;
    std::unordered_set<Node *> visited;
    nodes.emplace(from);
    uint64_t count = 0;
    while (!nodes.empty() && ((count++) < max_depth)) {
        auto n = nodes.top();
        nodes.pop();
        if (n->id == to->id) {
            return true;
        }
        auto const &edges = n->edges_to;
        for (auto const &nn : edges) {
            if (visited.find(nn->to) != visited.end()) continue;
            nodes.push(nn->to);
        }
        visited.emplace(n);
    }
    return false;
}

bool Graph::has_path(Node *from, Node *to, const std::function<bool(const Edge *)> &cond) {
    // DFS based search
    std::stack<Node *> nodes;
    std::unordered_set<Node *> visited;
    nodes.emplace(from);
    while (!nodes.empty()) {
        auto n = nodes.top();
        nodes.pop();
        if (n->id == to->id) {
            return true;
        }
        auto const &edges = n->edges_to;
        for (auto const &nn : edges) {
            if (visited.find(nn->to) != visited.end()) continue;
            bool add_cond = cond(nn.get());
            if (add_cond) nodes.push(nn->to);
        }
        visited.emplace(n);
    }
    return false;
}

Node *Graph::select(const std::string &name) {
    // this is a tree traversal search
    auto tokens = string::get_tokens(name, ".");
    std::queue<std::string> search_names;
    for (auto const &n : tokens) {
        search_names.push(n);
    }

    uint64_t i = 0;
    // copy the raw vector over

    if (cache_nodes_.size() != nodes_.size()) {
        cache_nodes_.reserve(nodes_.size());
        // just need to update the new ones
        std::transform(nodes_.begin() + cache_nodes_.size(), nodes_.end(),
                       std::back_inserter(cache_nodes_), [](const auto &ptr) { return ptr.get(); });
    }
    auto nodes = &cache_nodes_;

    while (i < nodes->size() && !search_names.empty()) {
        auto const &target_name = search_names.front();
        auto &node = (*nodes)[i];
        if (node->name == target_name) {
            search_names.pop();
            if (search_names.empty()) return node;
            // narrow the scope
            // reset search scope and counter
            i = 0;
            nodes = &node->children;
        } else {
            i++;
        }
    }

    return nullptr;
}

void Graph::identify_registers() {
    for (auto &node : nodes_) {
        // it has to be named
        if (node->name.empty()) continue;
        // has to be an variable
        if (node->type != NodeType::Net && node->type != NodeType::Variable) continue;
        bool non_blocking = true;
        for (auto const &edge : node->edges_from) {
            if (edge->from->has_type(NodeType::Assign) && edge->type == EdgeType::Blocking) {
                non_blocking = false;
                break;
            }
        }
        // it has to be non-blocking
        if (!non_blocking) continue;

        if (!node->edges_from.empty()) {
            // this is a registers
            node->type = node->type | NodeType::Register;
        }
    }
}

std::vector<Node *> Graph::get_registers() const {
    std::vector<Node *> result;
    // this is just a approximation
    result.reserve(static_cast<uint64_t>(std::sqrt(nodes_.size())));
    for (auto const &node : nodes_) {
        if (node->has_type(NodeType::Register)) {
            result.emplace_back(node.get());
        }
    }
    return result;
}

bool constant_driver(const Node *node, std::unordered_set<const Node *> &self_assignment_nodes,
                     std::unordered_set<const Edge *> &const_sources) {
    // we allow self loop
    self_assignment_nodes.emplace(node);
    auto const &edges = node->edges_from;
    if (edges.empty()) {
        // no visible driver
        return false;
    }
    bool result = true;
    for (auto const &edge : edges) {
        // if it is a slice, we need to go a skip?
        if (edge->has_type(EdgeType::Slice)) {
            continue;
        }

        auto const node_from = edge->from;
        // this is part of the loop group
        if (self_assignment_nodes.find(node_from) != self_assignment_nodes.end()) {
            continue;
        }
        if (node_from->has_type(NodeType::Assign) || node_from->has_type(NodeType::Variable)) {
            // need to figure out the source
            auto node_result = constant_driver(node_from, self_assignment_nodes, const_sources);
            if (!node_result) {
                result = false;
                break;
            }
        } else if (node_from->has_type(NodeType::Net)) {
            // NOTE::
            // 1. this is a herustics on how people write FSM that doesn't follow traditional
            // convention, e.g., additions
            // we only allow self loop with limited ops, such as add, and subtract
            // 2. if it's a net with name, i.e., wire/reg/logic, we continue the search
            if ((node_from->op != NetOpType::Ignore && node_from->edges_from.size() <= 2) ||
                !node_from->name.empty()) {
                auto node_result = constant_driver(node_from, self_assignment_nodes, const_sources);
                if (!node_result) {
                    result = false;
                    break;
                }
            } else {
                result = false;
                break;
            }
        } else if (node_from->has_type(NodeType::Control)) {
            // this is allowed as this is the node that controls whether to assign or not
            // but no recursive call
            continue;
        } else if (!node_from->has_type(NodeType::Constant)) {
            result = false;
            break;
        } else {
            assert_(node_from->type == NodeType::Constant, "node type has to be constant");
            const_sources.emplace(edge);
        }
    }

    // if all the edges are controls, then it shouldn't be a constant driver
    {
        uint32_t num_control = 0;
        for (auto const &edge : edges) {
            if (edge->from->type == NodeType::Control) {
                num_control++;
            }
        }
        if (num_control == edges.size()) result = false;
    }

    if (result) {
        if (const_sources.empty()) result = false;
    } else {
        const_sources.clear();
    }

    return result;
}

bool Graph::constant_driver(const Node *node) {
    std::unordered_set<const Node *> self_nodes;
    std::unordered_set<const Edge *> const_nodes;
    return fsm::constant_driver(node, self_nodes, const_nodes);
}

bool Graph::reachable(const Node *from, const Node *to) {
    // BFS search
    std::queue<const Node *> working_set;
    std::unordered_set<const Node *> visited;
    working_set.emplace(from);
    // edge case
    if (from->edges_to.empty()) return false;

    while (!working_set.empty()) {
        auto n = working_set.front();
        working_set.pop();
        if (n == to) return true;
        if (visited.find(n) != visited.end()) continue;
        visited.emplace(n);
        for (auto const &edge : n->edges_to) {
            working_set.emplace(edge->to);
        }
    }
    return false;
}

bool Graph::has_loop(const Node *node) { return reachable(node, node); }

bool reachable_control_loop(const Node *from, const Node *to) {
    // modified BFS search that contains at least one control node
    // to avoid memory blow up, this is done through two parts
    // 1. label all control nodes reachable from the origin nodes, and put all the control node
    //    into a set (unordered_set for fast query)
    // 2. do a BFS that check the path
    //    if the current head is a in the control node set, put all the new edges into the control
    //    node set as well. By doing so we don't have to keep trace of all the path traces.
    //    this is effectively union find by collapsing path trace
    std::unordered_set<const Node *> reachable_control_nodes;
    std::queue<const Node *> working_set;
    std::unordered_set<const Node *> visited;
    working_set.emplace(from);

    // edge case
    if (from->edges_to.empty()) return false;

    while (!working_set.empty()) {
        auto n = working_set.front();
        working_set.pop();
        if (n->has_type(NodeType::Control)) {
            reachable_control_nodes.emplace(n);
        }
        if (visited.find(n) != visited.end()) continue;
        visited.emplace(n);
        for (auto const &edge : n->edges_to) {
            working_set.emplace(edge->to);
        }
    }

    // second pass
    working_set = std::queue<const Node *>();
    visited = std::unordered_set<const Node *>();
    working_set.emplace(from);
    while (!working_set.empty()) {
        auto n = working_set.front();
        working_set.pop();
        if (n == to && reachable_control_nodes.find(n) != reachable_control_nodes.end()) {
            return true;
            // if not, continue the search
        } else {
            if (visited.find(n) != visited.end()) continue;
            visited.emplace(n);
            for (auto const &edge : n->edges_to) {
                auto nn = edge->to;
                // if we reached a control node and its connected nodes are not, we need to
                // add its connected nodes back to working set, that is, recolor the node
                if (reachable_control_nodes.find(n) != reachable_control_nodes.end() &&
                    reachable_control_nodes.find(nn) == reachable_control_nodes.end()) {
                    // flatten the set
                    reachable_control_nodes.emplace(nn);
                    // going to revisit the node since we have changed the path
                    if (visited.find(nn) != visited.end()) {
                        visited.erase(nn);
                    }
                }
                working_set.emplace(nn);
            }
        }
    }

    return false;
}

bool Graph::has_control_loop(const Node *node) { return reachable_control_loop(node, node); }

std::vector<const Node *> Graph::find_sinks(const Node *node, uint32_t depth) {
    std::unordered_set<const Node *> visited;
    std::queue<const Node *> working_set;
    working_set.emplace(node);
    std::unordered_map<const Node *, uint32_t> level_nodes = {{node, 0}};
    std::vector<const Node *> result;

    while (!working_set.empty()) {
        auto n = working_set.front();
        working_set.pop();
        if (visited.find(n) != visited.end()) {
            continue;
        } else {
            visited.emplace(n);
        }

        auto d = level_nodes.at(n);
        if (depth != 0 && d > depth) {
            continue;
        }
        uint32_t current_level = d + 1;
        for (auto const &edge : n->edges_to) {
            if (edge->has_type(EdgeType::Control)) continue;
            auto const nn = edge->to;
            level_nodes.emplace(nn, current_level);
            working_set.emplace(nn);
        }
        result.emplace_back(n);
    }

    return result;
}

std::unordered_set<const Edge *> Graph::get_constant_source(const Node *node) {
    std::unordered_set<const Node *> self_nodes;
    std::unordered_set<const Edge *> result;
    ::fsm::constant_driver(node, self_nodes, result);
    return result;
}

bool is_counter_(const Node *target, const Node *node) {
    // if there is any + or - based operator on the target node
    // first we do a search and figure out every assigned nodes
    // BFS based search
    std::queue<const Node *> working_set;
    std::unordered_set<const Node *> visited;
    working_set.emplace(node);
    while (!working_set.empty()) {
        auto n = working_set.front();
        working_set.pop();
        if (n == target) break;
        if (visited.find(n) != visited.end()) continue;
        visited.emplace(n);
        if (n->has_type(NodeType::Assign)) {
            auto const &edges_from = n->edges_from;
            for (auto const edge : edges_from) {
                if (!edge->has_type(EdgeType::Control)) {
                    auto nn = edge->from;
                    if (Graph::reachable(target, nn) &&
                        (nn->op == NetOpType::Add || nn->op == NetOpType::Subtract)) {
                        return true;
                    }
                }
            }
        }
        for (auto const &edge : n->edges_to) {
            working_set.emplace(edge->to);
        }
    }
    return false;
}

bool Graph::is_counter(const Node *node, const std::unordered_set<const Edge *> &edges) {
    // we do a filtering to speed up the process
    // if it is a counter, it's unlikely it will be mixed with explicit state
    std::unordered_map<int64_t, const Edge *> const_edges;
    for (auto const &edge : edges) {
        auto const node_from = edge->from;
        assert_(node_from->type == NodeType::Constant, "fsm state has to be driven by constant");
        if (const_edges.find(node_from->value) == const_edges.end()) {
            const_edges.emplace(node_from->value, edge);
        }
    }
    for (auto const &iter : const_edges) {
        auto const edge = iter.second;
        auto assign_to = edge->to;
        // net is only created
        if (assign_to->type == NodeType::Net) {
            return true;
        }
        assert_(assign_to->has_type(NodeType::Assign));
        assert_(assign_to->edges_to.size() == 1);
        auto edge_to = assign_to->edges_to.front().get();
        const Node *n = edge_to->to;
        auto r = is_counter_(node, n);
        if (r) return true;
    }
    return false;
}

bool Graph::in_direct_assign_chain(const Node *from, const Node *to) {
    if (from == to) return true;
    std::queue<const Node *> working_set;
    std::unordered_set<const Node *> visited;
    working_set.emplace(from);

    while (!working_set.empty()) {
        auto node = working_set.front();
        working_set.pop();
        if (visited.find(node) != visited.end()) continue;
        visited.emplace(node);

        for (auto const &edge : node->edges_to) {
            if (!edge->has_type(EdgeType::Control)) {
                auto n = edge->to;
                if (n == to) return true;
                if (n->has_type(NodeType::Assign)) {
                    // just to make sure that this is the only assignment we have
                    uint32_t num_direct_assign = 0;
                    for (auto edge_from : n->edges_from) {
                        if (edge_from->has_type(EdgeType::Blocking) ||
                            edge_from->has_type(EdgeType::NonBlocking))
                            num_direct_assign++;
                    }
                    if (num_direct_assign == 1) working_set.emplace(n);
                }
            }
        }
    }

    return false;
}

std::unordered_set<const Edge *> Graph::find_connection_cond(
    const Node *from, const std::function<bool(const Edge *)> &predicate) {
    std::unordered_set<const Edge *> result;
    std::queue<const Node *> working_set;
    std::unordered_set<const Node *> visited;
    working_set.emplace(from);

    while (!working_set.empty()) {
        auto node = working_set.front();
        working_set.pop();
        if (visited.find(node) != visited.end()) continue;
        visited.emplace(node);

        for (auto const &edge : node->edges_to) {
            working_set.emplace(edge->to);
            if (predicate(edge.get())) {
                result.emplace(edge.get());
            }
        }
    }
    return result;
}

std::vector<FSMResult> Graph::identify_fsms() { return identify_fsms(nullptr); }

std::vector<FSMResult> Graph::identify_fsms(const Node *top) {
    std::vector<FSMResult> result;

    // first it has to be a register
    identify_registers();
    auto registers = get_registers();
    tqdm bar;
    std::mutex mutex;
    auto num_cpus = get_num_cpus();
    cxxpool::thread_pool pool{num_cpus};
    std::vector<std::future<void>> tasks;
    tasks.reserve(registers.size());
    uint64_t count = 0;
    uint64_t num_registers = registers.size();

    for (auto reg : registers) {
        if (top && !reg->child_of(top)) continue;
        // I think the constant driver is faster?
        auto t = pool.push([reg, &mutex, &count, &bar, num_registers, &result]() -> void {
            auto const_src = Graph::get_constant_source(reg);
            mutex.lock();
            count++;
            bar.progress(count, num_registers);
            mutex.unlock();

            if (const_src.size() > 1) {
                // next one is to check if there is a constant loop
                bool control_loop = Graph::has_control_loop(reg);
                if (control_loop) {
                    // this is the fsm
                    FSMResult fsm(reg, const_src);
                    mutex.lock();
                    result.emplace_back(fsm);
                    mutex.unlock();
                }
            }
        });
        tasks.emplace_back(std::move(t));
    }

    for (auto &t : tasks) {
        t.wait();
    }
    for (auto &t : tasks) {
        t.get();
    }

    return result;
}

Node *Graph::copy_node(const Node *node, bool copy_connection) {
    auto n = add_node(get_free_id(), node->name);
    n->type = node->type;
    n->value = node->value;

    if (copy_connection) {
        // copy the connections as well, if specified
        for (auto const &edge : node->edges_to) {
            auto nn = edge->to;
            n->add_edge(nn, edge->type);
        }
        for (auto const edge : node->edges_from) {
            auto nn = edge->from;
            nn->add_edge(n, edge->type);
        }
    }

    return n;
}

std::pair<const Node *, const Node *> coupled_fsms(const Node *fsm_from, const Node *fsm_to,
                                                   bool fast_mode) {
    bool coupled = false;
    if (fast_mode) {
        coupled = Graph::reachable(fsm_from, fsm_to);
    } else {
        reachable_control_loop(fsm_from, fsm_to);
    }
    if (coupled) {
        return {fsm_from, fsm_to};
    } else {
        return {nullptr, nullptr};
    }
}

std::unordered_map<const Node *, std::unordered_set<const Node *>> Graph::group_fsms(
    const std::vector<FSMResult> &fsms, bool fast_mode) {
    std::unordered_map<const Node *, std::unordered_set<const Node *>> result;
    auto num_cpus = get_num_cpus();
    cxxpool::thread_pool pool{num_cpus};
    std::vector<std::future<std::pair<const Node *, const Node *>>> tasks;
    tasks.reserve(fsms.size() * fsms.size());

    for (uint64_t i = 0; i < fsms.size(); i++) {
        auto const &fsm_from = fsms[i].node();
        for (uint64_t j = 0; j < fsms.size(); j++) {
            if (i == j) continue;
            auto const &fsm_to = fsms[j].node();
            auto t = pool.push([=]() { return coupled_fsms(fsm_from, fsm_to, fast_mode); });
            tasks.emplace_back(std::move(t));
        }
    }

    for (auto &thread : tasks) {
        auto const [f, t] = thread.get();
        if (t) {
            result[f].emplace(t);
        }
    }

    return result;
}

}  // namespace fsm