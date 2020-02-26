#ifndef PASTAFARIAN_GRAPH_HH
#define PASTAFARIAN_GRAPH_HH

#include <unordered_map>

class Node {

};

class Graph {
public:

  Node* add_node(uint64_t key);

private:
  std::unordered_map<uint64_t, Node> nodes_;
};

#endif // PASTAFARIAN_GRAPH_HH
