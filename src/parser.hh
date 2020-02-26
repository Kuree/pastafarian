#ifndef PASTAFARIAN_PARSER_HH
#define PASTAFARIAN_PARSER_HH

#include "graph.hh"
#include <string>
#include <vector>

class Parser {
public:
  explicit Parser(Graph *graph) : graph_(graph) {}
  void parse(std::vector<std::string> &files);

private:
  Graph *graph_;

};

#endif // PASTAFARIAN_PARSER_HH
