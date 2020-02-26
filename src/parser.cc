#include "parser.hh"

#include <iostream>

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
    std::vector<std::string> f = {filename};
    parse(f, top);
}

void Parser::parse(const std::vector<std::string> &files, const std::string &top) {
    using namespace slang;
    SourceManager source_manager;

    PreprocessorOptions pre_prop_options;
    LexerOptions l_options;
    ParserOptions p_options;
    CompilationOptions c_options;
    Bag options;
    options.add(pre_prop_options);
    options.add(l_options);
    options.add(p_options);
    options.add(c_options);

    std::vector<SourceBuffer> buffers;
    for (auto const &filename : files) {
        auto buffer = source_manager.readSource(filename);
        if (!buffer) {
            throw std::runtime_error(::format("{0} does not exist", filename));
        }
        buffers.push_back(buffer);
    }
    if (buffers.empty()) {
        throw std::runtime_error("Input empty");
    }

    // compile the design
    Compilation compilation(options);
    for (auto const &buffer : buffers) {
        auto ast_tree = SyntaxTree::fromBuffer(buffer, source_manager, options);
        compilation.addSyntaxTree(ast_tree);
    }

    // get top
    auto const &def = compilation.getDefinition(top);
    if (!def) {
        throw std::runtime_error(::format("Unable to find {0}", top));
    }
    VariableVisitor visitor(graph_);
    def->visit(visitor);
}