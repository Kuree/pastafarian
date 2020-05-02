#include "parser.hh"

#include <iostream>
#include <optional>
#include <queue>

#include "fmt/format.h"
#include "simdjson/simdjson.h"
#include "source.hh"
#include "util.hh"

using fmt::format;

constexpr auto SUCCESS = simdjson::error_code::SUCCESS;

namespace fsm {

void parse_verilog(SourceManager &source) {
    // need to run slang to get the ast json
    // make sure slang exists
    // if SLANG is set in the env
    auto slang = Parser::get_slang();
    if (slang.empty()) throw std::runtime_error("Unable to find slang driver");
    // prepare for the slang arguments
    auto const &filenames = source.src_filenames();
    auto const &include_dirs = source.src_include_dirs();
    std::vector<std::string> args = {slang};
    args.reserve(1 + filenames.size() + 1 + include_dirs.size() + 2);
    // all the files
    args.insert(args.end(), filenames.begin(), filenames.end());
    // if we have include dirs
    if (!include_dirs.empty()) {
        args.emplace_back("-I");
        args.insert(args.end(), include_dirs.begin(), include_dirs.end());
    }
    // if we have any macros
    auto const &macros = source.macros();
    if (!macros.empty()) {
        std::vector<std::string> values;
        values.reserve(macros.size());
        for (auto const &[name, value] : macros) {
            auto v = ::format("{0}={1}", name, value);
            values.emplace_back(v);
        }
        auto v_str = string::join(values.begin(), values.end(), " ");
        args.emplace_back(::format("-D {0}", v_str));
    }
    // json output
    // get a random filename
    std::string temp_dir = fs::temp_directory_path();
    auto temp_filename = fs::join(temp_dir, "output.json");
    args.emplace_back("--ast-json");
    args.emplace_back(temp_filename);
    auto command = string::join(args.begin(), args.end(), " ");
    auto ret = std::system(command.c_str());
    if (ret) {
        throw std::runtime_error(
            ::format("Unable to parse {0}", string::join(filenames.begin(), filenames.end(), " ")));
    }
    source.set_json_filename(temp_filename);
}

template <class T>
Node *parse_dispatch(T value, Graph *g, Node *parent);
int64_t parse_num_literal(std::string_view str);

template <class T>
Node *parse_real_literal(T value, Graph *g) {
    auto type_node = value["type"];
    assert_(type_node.error == SUCCESS, "unable to find type in real_literal");
    auto type_str = type_node.as_string();
    assert_(std::string(type_str) == "real", "only real can be parsed");
    auto real_value = value["constant"];
    auto real_str = std::string(real_value.as_string());
    double real = 0;

    try {
        real = std::stod(real_str);
    } catch (...) {
        std::cerr << "Unable to parse " << real_str << std::endl;
    }

    auto node = g->add_node(g->get_free_id(), "", NodeType::Constant);
    node->value = static_cast<int64_t>(real);

    return node;
}

template <class T>
uint64_t get_address(T value) {
    auto addr_json = value["addr"];
    assert_(addr_json.error == SUCCESS, "addr not found in address");
    auto addr = addr_json.as_uint64_t();
    return addr;
}

uint64_t parse_internal_symbol(std::string_view symbol) {
    auto tokens = string::get_tokens(symbol, " ");
    assert_(tokens.size() == 2, "internal symbol has to be two tokens");
    return static_cast<uint64_t>(std::stoll(tokens[0]));
}

std::string parse_internal_symbol_name(std::string_view symbol) {
    auto tokens = string::get_tokens(symbol, " ");
    assert_(tokens.size() == 2, "internal symbol has to be two tokens");
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
    if (name_str[0] == 's') {
        // don't care about the sign
        name_str = name_str.substr(1);
    }
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
    try {
        auto r = std::stoll(name_str, nullptr, base);
        return r;
    } catch (std::out_of_range &) {
        return 0xFFFFFFFFFFFFFFFF;
    }
}

static bool has_parse_string_warning = false;
int64_t parse_string_literal(std::string_view str) {
    // convert to asci
    int64_t result = 0;
    if (str.size() > 8 && !has_parse_string_warning) {
        std::cerr << "Unable to cast long string literal (" << str << ")to integer" << std::endl;
        has_parse_string_warning = true;
    }
    for (uint64_t i = 0; i < str.size() && i < 8; i++) {
        result = result | (str[i]) << (8 * i);  // NOLINT
    }
    return result;
}

template <class T>
Node *parse_param(T value, Graph *g, Node *parent) {
    auto addr = get_address(value);

    auto name_json = value["name"];
    assert_(name_json.error == SUCCESS, "name not found in parameter");
    auto name = std::string(name_json.as_string());

    auto v_json = value["value"];
    assert_(v_json.error == SUCCESS, "value not found in parameter");
    auto v_str = v_json.as_string();
    auto v = parse_num_literal(v_str);

    auto node = g->add_node(addr, name, NodeType::Constant, parent);
    node->value = v;

    // non-local module level parameter
    auto is_port_raw = value["isPort"];
    assert_(is_port_raw.error == SUCCESS, "isPort not found in parameter");
    if (is_port_raw.as_bool()) {
        // it's a port parameter, put it to the module definition
        if (parent->type == NodeType::Module) {
            parent->module_def->params.emplace(name, node);
        }
    }

    return node;
}

template <class T>
Node *parse_num_literal(T value, Graph *g) {
    auto value_json = value["constant"];
    bool string_literal = false;
    if (value_json.error != SUCCESS) {
        value_json = value["value"];
    }
    if (value_json.error != SUCCESS) {
        // literal?
        value_json = value["literal"];
        string_literal = true;
    }
    assert_(value_json.error == SUCCESS, "constant value not found in number literal");
    auto v = string_literal ? parse_string_literal(value_json.as_string())
                            : parse_num_literal(value_json.as_string());
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
    assert_(operand.error == SUCCESS, "operand not found in conversion");
    return parse_dispatch(operand, g, nullptr);
}

template <class T>
Node *parse_event_list(T value, Graph *g, Node *parent) {
    auto events_raw = value["events"];
    assert_(events_raw.error == SUCCESS, "event list does not have events");
    auto events = events_raw.as_array();
    for (auto const &elem : events) {
        parse_dispatch(elem, g, parent);
    }
    return nullptr;
}

template <class T>
Node *parse_signal_event(T value, Graph *g, Node *parent) {
    auto expr_raw = value["expr"];
    assert_(expr_raw.error == SUCCESS, "cannot find expr from signal event");
    auto expr = parse_dispatch(expr_raw, g, parent);

    auto edge_raw = value["edge"];
    assert_(edge_raw.error == SUCCESS, "cannot find edge from signal event");
    auto edge = std::string(edge_raw.as_string());
    if (edge == "PosEdge") {
        expr->event_type = EventType::Posedge;
    } else if (edge == "NegEdge") {
        expr->event_type = EventType::Negedge;
    } else {
        assert_(edge == "None", "Unknown edge type " + edge);
        expr->event_type = EventType::None;
    }
    return nullptr;
}

template <class T>
Node *parse_binary_op(T value, Graph *g) {
    auto left_json = value["left"];
    auto right_json = value["right"];
    assert_(left_json.error == SUCCESS && right_json.error == SUCCESS,
            "error in binary op parsing");
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
        } else if (op == "Equality") {
            node->op = NetOpType::Equal;
        }
    }

    return node;
}

template <class T>
Node *parse_module(T &value, Graph *g, Node *parent) {
    auto name = std::string(value["name"].as_string());
    auto addr = get_address(value);
    auto n = g->add_node(addr, name, NodeType::Module, parent);

    // definition stuff
    auto definition = value["definition"].as_string();
    auto def_name = parse_internal_symbol_name(definition);
    auto module_def = std::make_unique<ModuleDefInfo>();
    module_def->name = def_name;
    n->module_def = std::move(module_def);

    // parse inner members
    assert_(value["members"].error == SUCCESS, "member not found in module");
    auto members = value["members"].as_array();
    for (auto const &member : members) {
        parse_dispatch(member, g, n);
    }

    return n;
}

template <class T>
Node *parse_member_access(T &value, Graph *g) {
    auto field = value["field"];
    assert_(field.error == SUCCESS, "unable to find field from member access");
    auto field_str = std::string(field.as_string());
    field_str = parse_internal_symbol_name(field_str);
    auto v = value["value"];
    Node *n = parse_dispatch(v, g, nullptr);
    assert_(n->members.find(field_str) != n->members.end(), "unable to find " + field_str);
    auto child = n->members.at(field_str);
    return child;
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
    assert_(list.error == SUCCESS, "list not found in statement list");
    auto stmts = list.as_array();
    for (auto const &stmt : stmts) {
        parse_dispatch(stmt, g, parent);
    }
    return nullptr;
}

template <class T>
Node *parse_expression_stmt(T value, Graph *g, Node *parent) {
    auto expr = value["expr"];
    assert_(expr.error == SUCCESS, "expr not found in expression statement");

    return parse_dispatch(expr, g, parent);
}

template <class T>
Node *parse_timed(T value, Graph *g, Node *parent) {
    auto stmt = value["stmt"];
    assert_(stmt.error == SUCCESS, "stmt not found in timed statement");

    // get edge trigger type
    auto timing = value["timing"];
    if (timing.error == SUCCESS) {
        if constexpr (std::is_same_v<typeof(timing), simdjson::document::element_result<
                                                         simdjson::document::element>>) {
            auto v = timing.value;
            if (v.is_object()) {
                parse_dispatch(timing, g, parent);
            }
        }
    }

    return parse_dispatch(stmt, g, parent);
}

template <class T>
Node *parse_conditional(T value, Graph *g, Node *parent) {
    auto cond = value["cond"];
    assert_(cond.error == SUCCESS, "cond not found in conditional statement");
    auto cond_node_parent = parse_dispatch(cond, g, parent);
    assert_(cond_node_parent != nullptr, "cond is null for conditional statement");
    // this will be a control node
    auto cond_node = g->add_node(g->get_free_id(), "", parent);
    cond_node_parent->add_edge(cond_node);
    cond_node->type = NodeType::Control;

    // true part
    auto if_true = value["ifTrue"];
    assert_(if_true.error == SUCCESS, "ifTrue not found for conditional statement");
    parse_dispatch(if_true, g, cond_node);

    auto if_false = value["ifFalse"];
    if (if_false.error == SUCCESS) {
        // put a negate on the cond code
        auto negate = g->add_node(g->get_free_id(), "", cond_node);
        negate->op = NetOpType::LogicalNot;
        negate->type = NodeType::Control;
        cond_node->add_edge(negate, EdgeType::False);
        parse_dispatch(if_false, g, negate);
    }

    if (parent && parent->has_type(NodeType::Control)) {
        parent->add_edge(cond_node, EdgeType::Control);
        cond_node->parent = parent;
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
        assert_(n != nullptr, "item is null for concatenation");
        n->add_edge(node);
    }

    return node;
}

template <class T>
Node *parse_ternary(T value, Graph *g) {
    auto pred = value["pred"];
    assert_(pred.error == SUCCESS, "pred is null for ternary operator");

    auto left = value["left"];
    auto right = value["right"];
    assert_(left.error == SUCCESS && right.error == SUCCESS, "error in ternary operator");

    auto pred_node = parse_dispatch(pred, g, nullptr);
    auto left_node = parse_dispatch(left, g, nullptr);
    auto right_node = parse_dispatch(right, g, nullptr);

    // create a control node
    auto node = g->add_node(g->get_free_id(), "", NodeType::Control);
    pred_node->add_edge(node);

    auto control_assign_node =
        g->add_node(g->get_free_id(), "", NodeType::Control | NodeType::Assign);
    node->add_edge(control_assign_node);
    control_assign_node->op = NetOpType::Ternary;

    left_node->add_edge(control_assign_node);
    right_node->add_edge(control_assign_node);

    return control_assign_node;
}

template <class T>
Node *parse_unary(T value, Graph *g) {
    auto op = value["operand"];
    assert_(op.error == SUCCESS, "operand is null for unary");
    auto op_node = parse_dispatch(op, g, nullptr);
    auto node = g->add_node(g->get_free_id(), "");
    auto op_code_raw = value["op"];
    assert_(op_code_raw.error == SUCCESS, "op is null for unary");
    auto op_str = std::string(op_code_raw.as_string());
    if (op_str == "LogicalNot")
        node->op = NetOpType::LogicalNot;
    else if (op_str == "BinaryOr")
        node->op = NetOpType::BinaryOr;
    else if (op_str == "BinaryAnd")
        node->op = NetOpType::BinaryAnd;
    else if (op_str == "BitwiseNot")
        node->op = NetOpType::BitwiseNot;
    op_node->add_edge(node);
    return node;
}

template <class T>
Node *parse_replication(T value, Graph *g) {
    auto count = value["count"];
    assert_(count.error == SUCCESS, "count not found for replication");

    auto concat_json = value["concat"];
    assert_(concat_json.error == SUCCESS, "concat not fund replication concat");
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
    assert_(v_node != nullptr, "cannot parse value for element select");
    auto selector_node = parse_dispatch(selector, g, nullptr);
    assert_(v_node != nullptr, "cannot parse selector for element select");

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
        for (auto const &arg : arguments) {
            auto arg_node = parse_dispatch(arg, g, call_node);
            arg_node->add_edge(call_node);
        }
    }

    return call_node;
}

template <class T>
Node *parse_case(T value, Graph *g, Node *parent) {
    auto items = value["items"];
    assert_(items.error == SUCCESS, "items not found in case statement");

    auto cond_node = value["expr"];
    assert_(cond_node.error == SUCCESS, "expr not found in case statement");
    auto cond = parse_dispatch(cond_node, g, parent);

    auto const &item_array = items.as_array();
    for (auto const &item : item_array) {
        auto expressions = item["expressions"];
        assert_(expressions.error == SUCCESS, "expressions not found in case item");
        auto const &exprs = expressions.as_array();
        std::vector<Node *> nodes;
        for (auto const &expr : exprs) {
            auto expr_node = parse_dispatch(expr, g, parent);
            assert_(expr_node != nullptr, "cannot parse expr in case item");
            nodes.emplace_back(expr_node);
        }
        assert_(!nodes.empty(), "expressions empty in case item");

        // link all the expressions with the new control node
        auto expr_node = g->add_node(g->get_free_id(), "", parent);
        for (auto const &e : nodes) {
            e->add_edge(expr_node);
        }

        // control node with the condition variable
        auto control_node = g->add_node(g->get_free_id(), "", parent);
        control_node->type = NodeType::Control;
        control_node->op = NetOpType::Equal;
        expr_node->add_edge(control_node);
        cond->add_edge(control_node);

        // stmt
        auto stmt = item["stmt"];
        assert_(stmt.error == SUCCESS, "stmt not found in case statement item");
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
void add_assignment_node(T value, Graph *g, Node *parent, const uint64_t addr, Node *left_node,
                         Node *right_node) {
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
    n->parent = parent;
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
    if (right_node->members.empty()) {
        add_assignment_node(value, g, parent, addr, left_node, right_node);
    } else {
        assert_(right_node->members.size() == left_node->members.size(),
                "only packed struct to packed struct allowed");
        for (auto const &[var_name, node_l] : left_node->members) {
            assert_(right_node->members.find(var_name) != right_node->members.end(),
                    ::format("unable to find {0} form {1}", var_name, right_node->name));
            auto node_r = right_node->members.at(var_name);
            add_assignment_node(value, g, parent, addr, node_l, node_r);
        }
    }

    return nullptr;
}

template <class T>
Node *parse_gate(T, Graph *, Node *) {
    // TODO: slang doesn't seem to parse gate properly
    return nullptr;
}

template <class T>
Node *parse_continuous_assignment(T value, Graph *g, Node *parent) {
    auto const &assignment = value["assignment"];
    assert_(assignment.error == SUCCESS, "assignment not found in continuous assignment");
    return parse_assignment(assignment, g, parent);
}

template <class T>
Node *parse_generate_block(T value, Graph *g, Node *parent) {
    auto instantiated = value["isInstantiated"];
    auto instantiated_value = instantiated.as_bool();
    if (!instantiated_value) return nullptr;
    auto members = value["members"].as_array();

    for (auto const &stmt : members) {
        parse_dispatch(stmt, g, parent);
    }

    return nullptr;
}

template <class T>
Node *parse_genvar(T value, Graph *g, Node *parent) {
    // add it to parent
    auto name = value["name"];
    auto name_str = std::string(name.as_string());
    auto node = g->add_node(g->get_free_id(), name_str);
    parent->members.emplace(name_str, node);

    return node;
}

template <class T>
Node *parse_generated_block_array(T value, Graph *g, Node *parent) {
    // we instantiate a "fake" module
    auto name = value["name"];
    assert_(name.error == SUCCESS, "cannot find name in generated block array");
    std::string_view name_str = name.as_string();
    auto members_raw = value["members"];
    assert_(members_raw.error == SUCCESS, "unable to access members from block array");
    auto members = members_raw.as_array();
    if (!name_str.empty()) {
        for (auto member : members) {
            auto kind = member["kind"];
            auto kind_str = std::string(kind.as_string());
            assert_(kind_str == "GenerateBlock", "none generate block found in block array");
            auto members_raw_ = member["members"];
            auto members_ = members_raw_.as_array();
            std::optional<int> index;
            for (auto member_ : members_) {
                auto kind_ = member_["kind"];
                if (std::string(kind_.as_string()) == "Parameter") {
                    auto name_raw_ = member_["name"];
                    auto name_ = std::string(name_raw_);
                    // find members
                    if (parent->members.find(name_) != parent->members.end()) {
                        auto param = parse_param(member_, g, nullptr);
                        index = param->value;
                        break;
                    }
                }
            }
            if (!index) {
                std::cerr << "Unable to parse blocks from " << parent->handle_name() << "."
                          << name_str << std::endl;
            } else {
                auto module_name = ::format("{0}[{1}]", name_str, *index);
                auto module = g->add_node(g->get_free_id(), module_name, NodeType::Module);
                parent->children.emplace_back(module);
                module->parent = parent;

                // parse the member
                parse_generate_block(member, g, module);
            }
        }

    } else {
        // cannot access the genvar block, disable the parsing
        std::cerr << "Unable to find lable name for generated block array from "
                  << parent->handle_name() << std::endl;
    }
    return nullptr;
}

struct ParseNode {
    ParseNode *parent = nullptr;
    std::vector<ParseNode *> children;
    std::string name;
};

ParseNode *get_parse_node(std::vector<std::unique_ptr<ParseNode>> &nodes) {
    auto p = std::make_unique<ParseNode>();
    auto ptr = p.get();
    nodes.emplace_back(std::move(p));
    return ptr;
}

void parse_struct_str(const std::string &string, Node *root, Graph *g) {
    std::vector<std::unique_ptr<ParseNode>> nodes;
    ParseNode *parent = nullptr;
    std::string name;

    for (auto const c : string) {
        switch (c) {
            case '{': {
                name = "";
                auto new_node = get_parse_node(nodes);
                if (parent) parent->children.emplace_back(new_node);
                new_node->parent = parent;
                parent = new_node;
                break;
            }
            case ' ': {
                name = "";
                break;
            }
            case ';': {
                // a member
                assert_(parent != nullptr, "parsing state error");
                if (!parent->children.empty() && parent->children.back()->name.empty()) {
                    parent->children.back()->name = name;
                } else {
                    // new member
                    auto new_node = get_parse_node(nodes);
                    parent->children.emplace_back(new_node);
                    new_node->parent = parent;
                    new_node->name = name;
                }
                name = "";
                break;
            }
            case '}': {
                if (parent->parent) parent = parent->parent;
                break;
            }
            default: {
                name += c;
            }
        }
    }
    std::queue<ParseNode *> working_set;
    working_set.emplace(parent);
    std::unordered_map<ParseNode *, Node *> node_map = {{parent, root}};
    while (!working_set.empty()) {
        auto p_n = working_set.front();
        working_set.pop();
        auto g_n = node_map.at(p_n);
        for (auto const c_n : p_n->children) {
            assert_(!c_n->name.empty(), "member name empty");
            if (g_n->members.find(c_n->name) != g_n->members.end()) continue;
            auto new_node = g->add_node(g->get_free_id(), c_n->name);
            g_n->members.emplace(new_node->name, new_node);
            g_n->children.emplace_back(new_node);
            new_node->parent = g_n;
            node_map.emplace(c_n, new_node);

            working_set.emplace(c_n);
        }
    }
}

template <class T>
void parse_complex_struct(Graph *g, Node *node, const T &elem) {
    std::string target_str;
    if constexpr (std::is_same_v<T, std::string>) {
        target_str = elem;
    } else {
        auto target_ = elem["target"];
        if (target_.error == SUCCESS) {
            target_str = std::string(target_.as_string());
        } else {
            if (elem.is_string()) {
                target_str = std::string(elem.as_string());
            }
        }
    }

    if (!target_str.empty()) {
        const static std::string PACKED_STRUCT = "struct packed";
        auto target_pos = target_str.find(PACKED_STRUCT);
        if (target_pos != std::string::npos) {
            parse_struct_str(target_str, node, g);
        }
    }
}

template <class T>
Node *parse_net(T value, Graph *g, Node *parent) {
    auto name_json = value["name"];
    assert_(name_json.error == SUCCESS, "name not found in net");
    auto wire_str = value["type"];
    assert_(wire_str.error == SUCCESS, "net does not have type str");

    auto name = std::string(name_json.as_string());
    auto addr = get_address(value);

    auto kind_str = std::string(value["kind"].as_string());
    NodeType type = NodeType::Variable;

    auto n = g->add_node(addr, name, type, parent);
    if constexpr (std::is_same_v<typeof(wire_str),
                                 simdjson::document::element_result<simdjson::document::element>>) {
        auto elem = wire_str.value;
        bool complex_struct = false;
        if (elem.is_object()) {
            auto kind = elem["kind"];
            if (kind.error == SUCCESS) {
                auto kind_str_ = std::string(kind.as_string());
                if (kind_str_ == "TypeAlias") {
                    complex_struct = true;
                }
            }
        } else if (elem.is_string()) {
            n->wire_type = wire_str;
            if (n->wire_type.find('$') != std::string::npos) {
                complex_struct = true;
            }
        }
        if (complex_struct) {
            parse_complex_struct(g, n, elem);
        }
    }
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

    if (kind_str == "Port") {
        auto direction = value["direction"];
        assert_(direction.error == SUCCESS, "cannot find direction from port");
        auto direction_str = std::string(direction.as_string());
        if (direction_str == "Out")
            n->port_type = PortType::Output;
        else
            n->port_type = PortType::Input;
    }

    return n;
}

static std::unordered_set<std::string> don_t_care_kind = {  // NOLINT
    "TransparentMember",   "TypeAlias",     "StatementBlock",
    "Subroutine",          "EmptyArgument", "Empty",
    "VariableDeclaration", "ImplicitEvent", "Delay"};

template <class T>
Node *parse_dispatch(T value, Graph *g, Node *parent) {
    assert_(value["kind"].error == SUCCESS, "kind not find in node");
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
    } else if (ast_kind == "IntegerLiteral" || ast_kind == "StringLiteral" ||
               ast_kind == "UnbasedUnsizedIntegerLiteral") {
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
    } else if (ast_kind == "ForLoop" || ast_kind == "ForeverLoop") {
        return parse_for_loop(value, g, parent);
    } else if (ast_kind == "Call") {
        return parse_call(value, g, parent);
    } else if (ast_kind == "GenerateBlock") {
        parse_generate_block(value, g, parent);
    } else if (ast_kind == "EventList") {
        parse_event_list(value, g, parent);
    } else if (ast_kind == "SignalEvent") {
        parse_signal_event(value, g, parent);
    } else if (ast_kind == "MemberAccess") {
        return parse_member_access(value, g);
    } else if (ast_kind == "RealLiteral") {
        return parse_real_literal(value, g);
    } else if (ast_kind == "Gate") {
        return parse_gate(value, g, parent);
    } else if (ast_kind == "GenerateBlockArray") {
        return parse_generated_block_array(value, g, parent);
    } else if (ast_kind == "Genvar") {
        return parse_genvar(value, g, parent);
    } else {
        std::cerr << "Unable to parse AST node kind " << ast_kind << std::endl;
    }
    return nullptr;
}

void Parser::parse(const SourceManager &value) {
    auto filename = value.json_filename();
    if (simdjson::active_implementation->name() == "unsupported") {
        throw std::runtime_error("Unsupported CPU");
    }
    // parse the entire JSON
    auto [doc, error] = simdjson::document::parse(simdjson::get_corpus(filename));
    if (error) {
        throw std::runtime_error(::format("unable to parse the JSON file {0}", filename));
    }
    assert_(std::string(doc["name"].as_string()) == "$root", "invalid slang output");
    auto const &members = doc["members"].as_array();
    for (auto const member : members) {
        parse_dispatch(member, graph_, nullptr);
    }
    parser_result_ = value;
}

void Parser::parse(const std::string &filename) {
    SourceManager r;
    if (fs::get_ext(filename) == ".json") {
        r.set_json_filename(filename);
    } else {
        r = SourceManager(filename);
        parse_verilog(r);
    }

    parse(r);
    parser_result_ = r;
}

bool Parser::has_slang() { return !get_slang().empty(); }

std::string Parser::get_slang() {
    std::string slang;
    auto slang_char = std::getenv("SLANG");
    if (slang_char) {
        slang = std::string(slang_char);
    }
    if (slang.empty()) {
        slang = fs::which("slang");
        if (slang.empty()) {
            slang = fs::which("slang-driver");
        }
    }
    return slang;
}

}  // namespace fsm