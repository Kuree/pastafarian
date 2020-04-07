#ifndef PASTAFARIAN_PARSER_HH
#define PASTAFARIAN_PARSER_HH

#include <string>
#include <vector>

#include "graph.hh"
#include "util.hh"

namespace fsm {

class Parser {
public:
    explicit Parser(Graph *graph) : graph_(graph) {}
    void parse(const std::string &filename);
    void parse(const ParseResult &value);

    const ParseResult &parser_result() const { return parser_result_; }

private:
    Graph *graph_;
    ParseResult parser_result_;
};
}  // namespace fsm

#endif  // PASTAFARIAN_PARSER_HH
