#include "parser.hh"

#include <iostream>
#include <fstream>
#include <sstream>

#include "fmt/format.h"
#include "slang/compilation/Compilation.h"
#include "slang/diagnostics/TextDiagnosticClient.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/symbols/ASTSerializer.h"
#include "slang/symbols/ASTVisitor.h"
#include "slang/symbols/VariableSymbols.h"
#include "slang/syntax/SyntaxPrinter.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceManager.h"

using fmt::format;

class VariableVisitor : public slang::ASTVisitor<VariableVisitor> {
public:
    explicit VariableVisitor(Graph *graph) : graph_(graph) {}
    template <typename T>
    void handle(const T &t) {}

    void handle(const slang::VariableSymbol &symbol) { process_variable(symbol); }

private:
    void process_variable(const slang::VariableSymbol &symbol) {
        if (graph_->has_node(&symbol)) {
            return;
        } else {
            std::cout << symbol.name.data() << std::endl;
        }
    }

    Graph *graph_;
};

void Parser::parse(const std::string &filename, const std::string &top) {
    std::vector<std::string> f;
    f.emplace_back(filename);
    parse(f, top);
}

void Parser::parse(const std::vector<std::string> &files, const std::string &top) {
    using namespace slang;

    PreprocessorOptions pre_prop_options;
    LexerOptions l_options;
    ParserOptions p_options;
    CompilationOptions c_options;
    Bag options;
    options.add(pre_prop_options);
    options.add(l_options);
    options.add(p_options);
    options.add(c_options);

    // compile the design
    Compilation compilation(options);

    for (auto const &filename : files) {
        std::ifstream stream(filename);
        std::stringstream buf;
        buf << stream.rdbuf();
        auto const &src = buf.str();
        auto ast = SyntaxTree::fromText(src);
        compilation.addSyntaxTree(ast);
    }

    // get top
    auto &root = compilation.getRoot();
    printf("%ld\n", root.topInstances.size());
    auto const &def = compilation.getDefinition(top);
    if (!def) {
        throw std::runtime_error(::format("Unable to find {0}", top));
    }
    VariableVisitor visitor(graph_);
    def->visit(visitor);
}