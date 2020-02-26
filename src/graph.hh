#ifndef PASTAFARIAN_GRAPH_HH
#define PASTAFARIAN_GRAPH_HH

#include <unordered_map>

class Node {};

class Graph {
public:
    Node* add_node(const void* key);
    bool has_node(const void* key) { return nodes_.find(key) != nodes_.end(); }

private:
    std::unordered_map<const void*, Node> nodes_;
};

#endif  // PASTAFARIAN_GRAPH_HH
