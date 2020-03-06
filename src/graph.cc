#include "graph.hh"

#include <stack>
#include <queue>

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
    // this is O(n) algorithm since we allow multiple top layers in the graph
    auto tokens = string::get_tokens(name, ".");
    std::queue<std::string> search_names;
    for (auto const &n: tokens) {
        search_names.push(n);
    }

    uint64_t i = 0;
    while (i < nodes_.size() && !search_names.empty()) {
        auto const &target_name = search_names.front();
        auto &node = nodes_[i];
        if (node->name == target_name) {
            search_names.pop();
            if (search_names.empty())
                return node.get();
        }

        i++;
    }

    return nullptr;
}