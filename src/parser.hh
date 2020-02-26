#ifndef PASTAFARIAN_PARSER_HH
#define PASTAFARIAN_PARSER_HH

#include "graph.hh"
#include <string>
#include <vector>

class Parser {
public:
  explicit Parser(Graph *graph) : graph_(graph) {}
  void parse(const std::string &filename, const std::string &top);
  void parse(const std::vector<std::string> &files, const std::string &top);

private:
  Graph *graph_;

};

#endif // PASTAFARIAN_PARSER_HH
