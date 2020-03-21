#include "parser.hh"

#include <iostream>

#include "fmt/format.h"
#include "simdjson/simdjson.h"
#include "util.hh"

using fmt::format;

constexpr auto SUCCESS = simdjson::error_code::SUCCESS;

namespace fsm {

template <class T>
Node *parse_dispatch(T value, Graph *g, Node *parent);
int64_t parse_num_literal(std::string_view str);

template <class T>
uint64_t get_address(T value) {
    auto addr_json = value["addr"];
    assert_(addr_json.error == SUCCESS);
    auto addr = addr_json.as_uint64_t();
    return addr;
}

uint64_t parse_internal_symbol(std::string_view symbol) {
    auto tokens = string::get_tokens(symbol, " ");
    assert_(tokens.size() == 2);
    return static_cast<uint64_t>(std::stoll(tokens[0]));
}

std::string parse_internal_symbol_name(std::string_view symbol) {
    auto tokens = string::get_tokens(symbol, " ");
    assert_(tokens.size() == 2);
    return tokens[1];
}

template <class T>
Node *parse_named_value(T value, Graph *g) {
    // if it is a constant symbol
    auto constant = value["constant"];

    auto symbol = value["symbol"].as_string().value;
    auto symbol_addr = parse_internal_symbol(symbol);

    if (!g->has_node(symbol_addr) && constant.error == SUCCESS) {
        auto c = parse_num_literal(constant.as_string().value);
        // this is a named constant
        // use the name for the name so that when we reconstruct the FSM state transition graph
        // the name will be there
        auto name = parse_internal_symbol_name(symbol);
        auto node = g->add_node(symbol_addr, name, NodeType::Constant);
        node->value = c;
        return node;
    } else {
        // if the symbol doesn't exist, the graph will create one
        auto node = g->get_node(symbol_addr);
        return node;
    }
}

int64_t parse_num_literal(std::string_view str) {
    auto tokens = string::get_tokens(str, "'");
    // we don't care about the size
    auto name_str = tokens.back();
    uint32_t base;
    if (name_str[0] == 'b') {
        base = 2;
        name_str = name_str.substr(1);
    } else if (name_str[0] == 'h') {
        base = 16;
        name_str = name_str.substr(1);
    } else if (name_str[0] == 'o') {
        base = 8;
        name_str = name_str.substr(1);
    } else if (name_str[0] == 'd') {
        base = 10;
        name_str = name_str.substr(1);
    } else {
        base = 10;
    }
    if (name_str.find('x') != std::string::npos || name_str.find('z') != std::string::npos) {
        // don't care for now?
        return 0;
    }
    auto r = std::stoll(name_str, nullptr, base);
    return r;
}

template <class T>
Node *parse_param(T value, Graph *g, Node *parent) {
    auto addr = get_address(value);

    auto name_json = value["name"];
    assert_(name_json.error == SUCCESS);
    auto name = std::string(name_json.as_string());

    auto v_json = value["value"];
    assert_(v_json.error == SUCCESS);
    auto v_str = v_json.as_string();
    auto v = parse_num_literal(v_str);

    auto node = g->add_node(addr, name, NodeType::Constant, parent);
    node->value = v;
    return node;
}

template <class T>
Node *parse_num_literal(T value, Graph *g) {
    auto constant_json = value["constant"];
    assert_(constant_json.error == SUCCESS);
    auto v = parse_num_literal(constant_json.as_string());
    auto node = g->add_node(g->get_free_id(), "", NodeType::Constant);
    node->value = v;
    return node;
}

template <class T>
Node *parse_conversion(T value, Graph *g) {
    // we need to keep track of the conversion
    // for our purpose, we don't care about the actual resizing etc
    // and only care about the connectivity

    auto operand = value["operand"];
    assert_(operand.error == SUCCESS);
    return parse_dispatch(operand, g, nullptr);
}

template <class T>
Node *parse_binary_op(T value, Graph *g) {
    auto left_json = value["left"];
    auto right_json = value["right"];
    assert_(left_json.error == SUCCESS && right_json.error == SUCCESS);
    auto left = parse_dispatch(left_json, g, nullptr);
    auto right = parse_dispatch(right_json, g, nullptr);

    // create a new node that connect these two
    auto node = g->add_node(g->get_free_id(), "");
    left->add_edge(node);
    right->add_edge(node);

    // get the op
    auto op_json = value["op"];
    if (op_json.error == SUCCESS) {
        auto op = std::string(op_json.as_string());
        if (op == "Add") {
            node->op = NetOpType::Add;
        } else if (op == "Subtract") {
            node->op = NetOpType::Subtract;
        }
    }

    return node;
}

template <class T>
Node *parse_module(T &value, Graph *g, Node *parent) {
    auto name = std::string(value["name"].as_string());
    auto addr = get_address(value);
    auto n = g->add_node(addr, name, NodeType::Module, parent);

    // parse inner members
    assert_(value["members"].error == SUCCESS);
    auto members = value["members"].as_array();
    for (auto const &member : members) {
        parse_dispatch(member, g, n);
    }
    return n;
}

template <class T>
Node *parse_block(T value, Graph *g, Node *parent) {
    uint64_t addr;
    if (value["addr"].error == SUCCESS) {
        addr = get_address(value);
    } else {
        addr = g->get_free_id();
    }
    auto node = g->add_node(addr, "", parent);

    auto body = value["body"];
    parse_dispatch(body, g, parent);
    return node;
}

template <class T>
Node *parse_list(T value, Graph *g, Node *parent) {
    auto list = value["list"];
    assert_(list.error == SUCCESS);
    auto stmts = list.as_array();
    for (auto const &stmt : stmts) {
        parse_dispatch(stmt, g, parent);
    }
    return nullptr;
}

template <class T>
Node *parse_expression_stmt(T value, Graph *g, Node *parent) {
    auto expr = value["expr"];
    assert_(expr.error == SUCCESS);

    return parse_dispatch(expr, g, parent);
}

template <class T>
Node *parse_timed(T value, Graph *g, Node *parent) {
    auto stmt = value["stmt"];
    assert_(stmt.error == SUCCESS);

    return parse_dispatch(stmt, g, parent);
}

template <class T>
Node *parse_conditional(T value, Graph *g, Node *parent) {
    auto cond = value["cond"];
    assert_(cond.error == SUCCESS);
    auto cond_node = parse_dispatch(cond, g, parent);
    assert_(cond_node != nullptr);
    // this will be a control node
    cond_node->type = NodeType::Control;

    // true part
    auto if_true = value["ifTrue"];
    assert_(if_true.error == SUCCESS);
    parse_dispatch(if_true, g, cond_node);

    auto if_false = value["ifFalse"];
    if (if_false.error == SUCCESS) {
        parse_dispatch(if_false, g, cond_node);
    }

    if (parent && parent->has_type(NodeType::Control)) {
        parent->add_edge(cond_node);
    }

    return cond_node;
}

template <class T>
Node *parse_range_select(T value, Graph *g) {
    auto v = value["value"];
    auto left = value["left"];
    auto right = value["right"];
    auto v_node = parse_dispatch(v, g, nullptr);
    auto left_node = parse_dispatch(left, g, nullptr);
    auto right_node = parse_dispatch(right, g, nullptr);

    left_node->add_edge(v_node, EdgeType::Slice);
    right_node->add_edge(v_node, EdgeType::Slice);

    return v_node;
}

template <class T>
Node *parse_concat(T value, Graph *g) {
    auto operands = value["operands"];
    auto const &array = operands.as_array();

    auto node = g->add_node(g->get_free_id(), "");

    for (auto const &operand : array) {
        auto n = parse_dispatch(operand, g, nullptr);
        assert_(n != nullptr);
        n->add_edge(node);
    }

    return node;
}

template <class T>
Node *parse_ternary(T value, Graph *g) {
    auto pred = value["pred"];
    assert_(pred.error == SUCCESS);

    auto left = value["left"];
    auto right = value["right"];
    assert_(left.error == SUCCESS && right.error == SUCCESS);

    auto pred_node = parse_dispatch(pred, g, nullptr);
    auto left_node = parse_dispatch(left, g, nullptr);
    auto right_node = parse_dispatch(right, g, nullptr);

    // create a control node
    auto node = g->add_node(g->get_free_id(), "", NodeType::Control);
    pred_node->add_edge(node);

    auto control_assign_node =
        g->add_node(g->get_free_id(), "", NodeType::Control | NodeType::Assign);
    node->add_edge(control_assign_node);

    left_node->add_edge(control_assign_node);
    right_node->add_edge(control_assign_node);

    return control_assign_node;
}

template <class T>
Node *parse_unary(T value, Graph *g) {
    auto op = value["operand"];
    assert_(op.error == SUCCESS);
    auto op_node = parse_dispatch(op, g, nullptr);
    auto node = g->add_node(g->get_free_id(), "");
    op_node->add_edge(node);
    return node;
}

template <class T>
Node *parse_replication(T value, Graph *g) {
    auto count = value["count"];
    assert_(count.error == SUCCESS);
    auto num = parse_dispatch(count, g, nullptr);
    assert_(num->type == NodeType::Constant, "count has to be a number");

    auto concat_json = value["concat"];
    assert_(concat_json.error == SUCCESS);
    auto var = parse_dispatch(concat_json, g, nullptr);
    if (var->type == NodeType::Constant) {
        // TODO: get the width from the design and do proper calculation
        std::cerr << "WARNING: constant replication not supported";
    }
    return var;
}

template <class T>
Node *parse_element_select(T value, Graph *g) {
    auto v = value["value"];
    auto selector = value["selector"];

    auto v_node = parse_dispatch(v, g, nullptr);
    assert_(v_node != nullptr);
    auto selector_node = parse_dispatch(selector, g, nullptr);
    assert_(v_node != nullptr);

    selector_node->add_edge(v_node, EdgeType::Slice);

    return v_node;
}

template <class T>
Node *parse_for_loop(T value, Graph *g, Node *parent) {
    // since we don't keep track of slices, we don't actually need to expand the loop body
    // this is because if a sub-value is selected, the whole signal will be used to construct
    // the graph
    auto body = value["body"];
    return parse_dispatch(body, g, parent);
}

bool is_system_task(std::string_view subroutine_name) {
    auto tokens = string::get_tokens(subroutine_name, " ");
    return tokens.size() == 1 && tokens[0][0] == '$';
}

template <class T>
Node *parse_call(T value, Graph *g, Node *parent) {
    static std::unordered_set<std::string> checked_subroutines;
    // if it's not built-in tasks, we need to inline the function. However, it is not worth the
    // effort for now. we will just wire them together
    auto subroutine = value["subroutine"];
    auto subroutine_name = subroutine.as_string();
    if (!is_system_task(subroutine_name)) {
        auto tokens = string::get_tokens(subroutine_name, " ");
        auto name = tokens.back();
        if (checked_subroutines.find(name) == checked_subroutines.end()) {
            std::cerr << "Custom task/function " << name << " not supported" << std::endl;
            checked_subroutines.emplace(name);
        }
    }

    auto call_node = g->get_node(g->get_free_id());
    call_node->parent = parent;

    // get the arguments
    auto raw_arguments = value["arguments"];
    if (raw_arguments.error == SUCCESS) {
        auto arguments = raw_arguments.as_array();
        for (auto const &arg: arguments) {
            auto arg_node = parse_dispatch(arg, g, call_node);
            arg_node->add_edge(call_node);
        }
    }

    return call_node;
}

template <class T>
Node *parse_case(T value, Graph *g, Node *parent) {
    auto items = value["items"];
    assert_(items.error == SUCCESS);

    auto cond_node = value["expr"];
    assert_(cond_node.error == SUCCESS);
    auto cond = parse_dispatch(cond_node, g, parent);

    auto const &item_array = items.as_array();
    for (auto const &item : item_array) {
        auto expressions = item["expressions"];
        assert_(expressions.error == SUCCESS);
        auto const &exprs = expressions.as_array();
        std::vector<Node *> nodes;
        for (auto const &expr : exprs) {
            auto expr_node = parse_dispatch(expr, g, parent);
            assert_(expr_node != nullptr);
            nodes.emplace_back(expr_node);
        }
        assert_(!nodes.empty());

        // link all the expressions with the new control node
        auto expr_node = g->add_node(g->get_free_id(), "", parent);
        for (auto const &e : nodes) {
            e->add_edge(expr_node);
        }

        // control node with the condition variable
        auto control_node = g->add_node(g->get_free_id(), "", parent);
        control_node->type = NodeType::Control;
        expr_node->add_edge(control_node);
        cond->add_edge(control_node);

        // stmt
        auto stmt = item["stmt"];
        assert_(stmt.error == SUCCESS);
        parse_dispatch(stmt, g, control_node);
    }
    // default case
    auto default_case = value["defaultCase"];
    if (default_case.error == SUCCESS) {
        auto control_node = g->add_node(g->get_free_id(), "", parent);
        control_node->type = NodeType::Control;
        cond->add_edge(control_node);

        parse_dispatch(default_case, g, control_node);
    }

    if (parent && parent->has_type(NodeType::Control)) {
        parent->add_edge(cond, EdgeType::Control);
    }

    return cond;
}

template <class T>
Node *parse_assignment(T value, Graph *g, Node *parent) {
    auto const &left = value["left"];
    auto const &right = value["right"];
    // we create a new ID that's not in the symbol table
    // this is fine since the symbol table is a memory address which won't go beyond
    // half of uint64_t space
    auto const addr = g->get_free_id();
    auto left_node = parse_dispatch(left, g, parent);
    auto right_node = parse_dispatch(right, g, parent);
    if (!right_node) {
        // right is just the parent
        right_node = parent;
    }
    // create an assignment node
    auto n = g->add_node(addr, "", NodeType::Assign);
    right_node->add_edge(n, EdgeType::Blocking);
    if (right_node != parent && parent && parent->has_type(NodeType::Control)) {
        // it's a control edge
        parent->add_edge(n, EdgeType::Control);
    }
    auto non_blocking = value["isNonBlocking"].as_bool();
    auto edge_type = non_blocking ? EdgeType::NonBlocking : EdgeType::Blocking;
    n->add_edge(left_node, edge_type);
    return nullptr;
}

template <class T>
Node *parse_continuous_assignment(T value, Graph *g, Node *parent) {
    auto const &assignment = value["assignment"];
    assert_(assignment.error == SUCCESS);
    return parse_assignment(assignment, g, parent);
}

template<class T>
Node *parse_generate_block(T value, Graph *g, Node *parent) {
    auto instantiated = value["isInstantiated"];
    auto instantiated_value = instantiated.as_bool();
    if (!instantiated_value) return nullptr;
    auto members = value["members"].as_array();

    for (auto const &stmt: members) {
        parse_dispatch(stmt, g, parent);
    }

    return nullptr;
}

template <class T>
Node *parse_net(T value, Graph *g, Node *parent) {
    auto name_json = value["name"];
    assert_(name_json.error == SUCCESS);

    auto name = std::string(name_json.as_string());
    auto addr = get_address(value);

    auto kind_str = std::string(value["kind"].as_string());
    NodeType type = NodeType::Net;
    if (kind_str == "Variable") {
        type = NodeType::Variable;
    }

    auto n = g->add_node(addr, name, type, parent);
    if (value["internalSymbol"].error == SUCCESS) {
        auto symbol = value["internalSymbol"].as_string();
        auto symbol_addr = parse_internal_symbol(symbol);
        g->alias_node(symbol_addr, n);
    }
    if (value["externalConnection"].error == SUCCESS) {
        auto const &connection = value["externalConnection"];
        auto node = parse_dispatch(connection, g, n);
        if (node) {
            // NOTICE:
            // if the external connection is not an assignment, it will return nullptr
            // add an assignment edge
            auto assign = g->add_node(g->get_free_id(), "", NodeType::Assign);
            node->add_edge(assign);
            assign->add_edge(n);
        }
    }
    return n;
}

static std::unordered_set<std::string> don_t_care_kind = {"TransparentMember",  // NOLINT
                                                          "TypeAlias", "StatementBlock",
                                                          "Subroutine"};

template <class T>
Node *parse_dispatch(T value, Graph *g, Node *parent) {
    assert_(value["kind"].error == SUCCESS);
    auto ast_kind = std::string(value["kind"].as_string());
    if (ast_kind == "CompilationUnit" || don_t_care_kind.find(ast_kind) != don_t_care_kind.end()) {
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
    } else if (ast_kind == "ContinuousAssign") {
        return parse_continuous_assignment(value, g, parent);
    } else if (ast_kind == "Parameter") {
        return parse_param(value, g, parent);
    } else if (ast_kind == "BinaryOp") {
        return parse_binary_op(value, g);
    } else if (ast_kind == "Conversion") {
        return parse_conversion(value, g);
    } else if (ast_kind == "ProceduralBlock" || ast_kind == "Block") {
        return parse_block(value, g, parent);
    } else if (ast_kind == "Timed") {
        return parse_timed(value, g, parent);
    } else if (ast_kind == "ExpressionStatement") {
        return parse_expression_stmt(value, g, parent);
    } else if (ast_kind == "List") {
        return parse_list(value, g, parent);
    } else if (ast_kind == "Conditional") {
        return parse_conditional(value, g, parent);
    } else if (ast_kind == "IntegerLiteral" || ast_kind == "StringLiteral") {
        return parse_num_literal(value, g);
    } else if (ast_kind == "Case") {
        return parse_case(value, g, parent);
    } else if (ast_kind == "RangeSelect") {
        return parse_range_select(value, g);
    } else if (ast_kind == "Concatenation") {
        return parse_concat(value, g);
    } else if (ast_kind == "ElementSelect") {
        return parse_element_select(value, g);
    } else if (ast_kind == "ConditionalOp") {
        return parse_ternary(value, g);
    } else if (ast_kind == "UnaryOp") {
        return parse_unary(value, g);
    } else if (ast_kind == "Replication") {
        return parse_replication(value, g);
    } else if (ast_kind == "ForLoop") {
        return parse_for_loop(value, g, parent);
    } else if (ast_kind == "Call") {
        return parse_call(value, g, parent);
    } else if (ast_kind == "GenerateBlock") {
        parse_generate_block(value, g, parent);
    } else {
        std::cerr << "Unable to parse AST node kind " << ast_kind << std::endl;
    }
    return nullptr;
}

void Parser::parse(const std::string &filename) {
    // parse the entire JSON
    auto [doc, error] = simdjson::document::parse(simdjson::get_corpus(filename));
    if (error) {
        throw std::runtime_error(::format("unable to parse the JSON file {0}"));
    }
    assert_(std::string(doc["name"].as_string()) == "$root");
    auto const &members = doc["members"].as_array();
    for (auto const &member : members) {
        parse_dispatch(member, graph_, nullptr);
    }
}

}  // namespace fsm