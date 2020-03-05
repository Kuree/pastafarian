#include "parser.hh"

#include <cassert>
#include <iostream>

#include "fmt/format.h"
#include "simdjson.h"
#include "util.hh"

using fmt::format;

template <typename T>
Node *parse_dispatch(const rapidjson::GenericValue<T> &value, Graph *g, Node *parent);

uint64_t parse_internal_symbol(const std::string &symbol) {
    auto tokens = string::get_tokens(symbol, " ");
    assert(tokens.size() == 2);
    return static_cast<uint64_t>(std::stoll(tokens[0]));
}

template <typename T>
Node *parse_named_value(const rapidjson::GenericValue<T> &value, Graph *g) {
    auto symbol = value["symbol"].GetString();
    auto symbol_addr = parse_internal_symbol(symbol);
    // if the symbol doesn't exist, the graph will create one
    auto node = g->get_node(symbol_addr);
    return node;
}

template <typename T>
Node *parse_module(const rapidjson::GenericValue<T> &value, Graph *g, Node *parent) {
    auto name = value["name"].GetString();
    auto addr = value["addr"].GetUint64();
    auto n = g->add_node(addr, name, NodeType::Module, parent);

    // parse inner members
    assert(value["members"].IsArray());
    auto members = value["members"].GetArray();
    for (auto const &member : members) {
        parse_dispatch(member, g, n);
    }
    return n;
}

template <typename T>
Node *parse_assignment(const rapidjson::GenericValue<T> &value, Graph *g, Node *parent) {
    assert(value.HasMember("left"));
    assert(value.HasMember("right"));
    auto const &left = value["left"];
    auto const &right = value["right"];
    auto left_node = parse_dispatch(left, g, parent);
    auto right_node = parse_dispatch(right, g, parent);
    if (!right_node) {
        // right is just the parent
        right_node = parent;
    }
    assert(value.HasMember("isNonBlocking"));
    auto non_blocking = value["isNonBlocking"].GetBool();
    auto edge_type = non_blocking ? EdgeType::NonBlocking : EdgeType::Blocking;
    right_node->add_edge(left_node, edge_type, nullptr);
    return nullptr;
}

template <typename T>
Node *parse_net(const rapidjson::GenericValue<T> &value, Graph *g, Node *parent) {
    auto name = value["name"].GetString();
    auto addr = value["addr"].GetUint64();
    auto n = g->add_node(addr, name, NodeType::Net, parent);
    if (value.HasMember("internalSymbol")) {
        auto symbol = value["internalSymbol"].GetString();
        auto symbol_addr = parse_internal_symbol(symbol);
        g->alias_node(symbol_addr, n);
    }
    if (value.HasMember("externalConnection")) {
        auto const &connection = value["externalConnection"];
        auto node = parse_dispatch(connection, g, n);
        // add an assignment edge
        if (connection["kind"] != "Assignment") n->add_edge(node);
    }
    return n;
}

template <typename T>
Node *parse_dispatch(const rapidjson::GenericValue<T> &value, Graph *g, Node *parent) {
    assert(value.HasMember("kind"));
    const std::string &ast_kind = value["kind"].GetString();
    if (ast_kind == "CompilationUnit") {
        // don't care
    } else if (ast_kind == "ModuleInstance") {
        // this is a module
        return parse_module(value, g, parent);
    } else if (ast_kind == "Port" || ast_kind == "Net" || ast_kind == "Variable") {
        return parse_net(value, g, parent);
    } else if (ast_kind == "NamedValue") {
        return parse_named_value(value, g);
    } else if (ast_kind == "Assignment") {
        return parse_assignment(value, g, parent);
    } else if (ast_kind == "EmptyArgument") {
        return nullptr;
    } else {
        std::cout << "Unable to parse AST node kind " << ast_kind << std::endl;
    }
    return nullptr;
}

void Parser::parse(const std::string &json_content) {
    // parse the entire JSON
    auto parser = simdjson::document::parser();
    auto [doc, error] = parser.parse(simdjson::padded_string(json_content));
    if (error) {
        throw std::runtime_error("unable to parse the JSON");
    }

    assert(doc.IsObject());
    assert(doc.HasMember("name"));
    assert(doc["name"].IsString());
    assert(doc["name"].GetString() == "$root");
    assert(doc.HasMember("members"));
    assert(doc["members"].IsArray());
    auto const &members = doc["members"].GetArray();
    for (auto const &member : members) {
        parse_dispatch(member, graph_, nullptr);
    }
}
