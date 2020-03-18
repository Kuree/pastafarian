#include "graph.hh"

#include <algorithm>
#include <cmath>
#include <queue>
#include <stack>

#include "util.hh"

namespace fsm {

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
            if (edge->type == EdgeType::Blocking) {
                non_blocking = false;
                break;
            }
        }
        // it has to be non-blocking
        if (!non_blocking) continue;

        if (!node->edges_from.empty()) {
            // this is a registers
            node->type = NodeType::Register;
        }
    }
}

std::vector<Node *> Graph::get_registers() const {
    std::vector<Node *> result;
    // this is just a approximation
    result.reserve(static_cast<uint64_t>(std::sqrt(nodes_.size())));
    for (auto const &node : nodes_) {
        if (node->type == NodeType::Register) {
            result.emplace_back(node.get());
        }
    }
    return result;
}

bool constant_driver(const Node *node, std::unordered_set<const Node *> &self_assignment_nodes) {
    // we allow self loop
    self_assignment_nodes.emplace(node);
    auto const &edges = node->edges_from;
    if (edges.empty()) {
        // no visible driver
        return false;
    }
    bool result = true;
    for (auto const &edge : edges) {
        auto const node_from = edge->from;
        // this is part of the loop group
        if (self_assignment_nodes.find(node_from) != self_assignment_nodes.end()) {
            continue;
        }
        if (node_from->has_type(NodeType::Assign) || node_from->has_type(NodeType::Net) ||
            node_from->has_type(NodeType::Variable)) {
            // need to figure out the source
            auto node_result = constant_driver(node_from, self_assignment_nodes);
            if (!node_result) {
                result = false;
                break;
            }
        } else if (node->has_type(NodeType::Assign) && node_from->has_type(NodeType::Control)) {
            // this is allowed as this is the node that controls whether to assign or not
            // but no recursive call
            continue;
        } else if (!node_from->has_type(NodeType::Constant)) {
            result = false;
            break;
        }
    }

    return result;
}

bool Graph::constant_driver(Node *node) {
    std::unordered_set<const Node *> self_nodes;
    return fsm::constant_driver(node, self_nodes);
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

std::unordered_set<const Node *> Graph::get_constant_source(const Node *node) {
    std::unordered_set<const Node *> result;
    std::queue<const Node *> working_set;
    std::unordered_set<const Node *> visited;
    working_set.emplace(node);

    while (!working_set.empty()) {
        auto n = working_set.front();
        working_set.pop();

        if (visited.find(n) != visited.end()) continue;
        visited.emplace(n);

        if (n->has_type(NodeType::Constant)) {
            result.emplace(n);
        } else if (n->has_type(NodeType::Assign) || n == node) {
            for (auto const &edge : n->edges_from) {
                if (!edge->has_type(EdgeType::Control))
                    working_set.emplace(edge->from);
            }
        }
    }

    return result;
}

}  // namespace fsm