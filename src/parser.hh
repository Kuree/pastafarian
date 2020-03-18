#ifndef PASTAFARIAN_PARSER_HH
#define PASTAFARIAN_PARSER_HH

#include <string>
#include <vector>

#include "graph.hh"

namespace fsm {

class Parser {
public:
    explicit Parser(Graph *graph) : graph_(graph) {}
    void parse(const std::string &filename);

private:
    Graph *graph_;
};
}  // namespace fsm

#endif  // PASTAFARIAN_PARSER_HH
