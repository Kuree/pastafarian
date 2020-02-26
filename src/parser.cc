#include "parser.hh"

#include "fmt/format.h"
#include "slang/compilation/Compilation.h"
#include "slang/diagnostics/TextDiagnosticClient.h"
#include "slang/parsing/Preprocessor.h"
#include "slang/symbols/ASTSerializer.h"
#include "slang/symbols/CompilationUnitSymbols.h"
#include "slang/syntax/SyntaxPrinter.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceManager.h"

using fmt::format;

void Parser::parse(std::vector<std::string> &files) {
    using namespace slang;
    SourceManager source_manager;

    Bag options{};
    std::vector<SourceBuffer> buffers;
    for (auto const &filename : files) {
        SourceBuffer buffer = source_manager.readSource(filename);
        if (!buffer) {
            throw std::runtime_error(::format("{0} does not exist", filename));
        }
        buffers.emplace_back(buffer);
    }
    if (buffers.empty()) {
        throw std::runtime_error("Input empty");
    }

    // compile the design

    Compilation compilation(options);
    for (auto const &buffer: buffers) {
        compilation.addSyntaxTree(SyntaxTree::fromBuffer(buffer, source_manager, options));
    }

    // root
    auto root = compilation.getRoot();
}