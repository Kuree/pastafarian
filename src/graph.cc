#include "graph.hh"

Node* Graph::add_node(const void* key) {
    auto r = nodes_.emplace(key, Node());
    return &r.first->second;
}