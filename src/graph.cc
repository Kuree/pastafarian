#include "graph.hh"


Node * Graph::add_node(uint64_t key) {
  auto r = nodes_.emplace(key, Node());
  return &r.first->second;
}