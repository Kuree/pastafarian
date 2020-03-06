#include "graph.hh"

#include <stack>
#include <queue>
#include <algorithm>

#include "util.hh"

Node * Graph::get_node(uint64_t key) {
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
    std::stack<Node*> nodes;
    nodes.emplace(from);
    uint64_t count = 0;
    while (!nodes.empty() && ((count++) < max_depth)) {
        auto n = nodes.top();
        nodes.pop();
        if (n == to) {
            return true;
        }
        auto const &edges = n->edges_to;
        for (auto const &nn: edges) {
            nodes.push(nn->to);
        }
    }
    return false;
}

Node * Graph::select(const std::string &name) {
    // this is a tree traversal search
    auto tokens = string::get_tokens(name, ".");
    std::queue<std::string> search_names;
    for (auto const &n: tokens) {
        search_names.push(n);
    }

    uint64_t i = 0;
    // copy the raw vector over

    if (cache_nodes_.size() != nodes_.size()) {
        cache_nodes_.reserve(nodes_.size());
        std::transform(nodes_.begin(), nodes_.end(), std::back_inserter(cache_nodes_), [](const auto &ptr) {
          return ptr.get();
        });
    }
    auto nodes = &cache_nodes_;

    while (i < nodes->size() && !search_names.empty()) {
        auto const &target_name = search_names.front();
        auto &node = (*nodes)[i];
        if (node->name == target_name) {
            search_names.pop();
            if (search_names.empty())
                return node;
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