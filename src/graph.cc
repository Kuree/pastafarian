#include "graph.hh"

#include <algorithm>
#include <cmath>
#include <queue>
#include <stack>

#include "util.hh"

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
        if (node->type != NodeType::Net) continue;
        for (auto const &edge : node->edges_from) {
            if (edge->type == EdgeType::Blocking) {
                break;
            }
        }
        if (!node->edges_from.empty()) {
            // this is a registers
            node->type = NodeType::Register;
        }
    }
}

std::vector<Node *> Graph::get_registers() const {
    std::vector<Node*> result;
    // this is just a approximation
    result.reserve(static_cast<uint64_t>(std::sqrt(nodes_.size())));
    for (auto const &node: nodes_) {
        if (node->type == NodeType::Register) {
            result.emplace_back(node.get());
        }
    }
    return result;
}