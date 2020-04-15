#ifndef PASTAFARIAN_PARSER_HH
#define PASTAFARIAN_PARSER_HH

#include <string>
#include <vector>

#include "graph.hh"
#include "source.hh"

namespace fsm {

void parse_verilog(SourceManager &source);

class Parser {
public:
    explicit Parser(Graph *graph) : graph_(graph) {}
    void parse(const std::string &filename);
    void parse(const SourceManager &value);

    [[nodiscard]] const SourceManager &parser_result() const { return parser_result_; }

    [[nodiscard]] static bool has_slang();
    [[nodiscard]] static std::string get_slang();

private:
    Graph *graph_;
    SourceManager parser_result_;
};
}  // namespace fsm

#endif  // PASTAFARIAN_PARSER_HH
