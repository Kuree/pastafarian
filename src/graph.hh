#ifndef PASTAFARIAN_GRAPH_HH
#define PASTAFARIAN_GRAPH_HH

#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

namespace fsm {

enum class NodeType {
    Constant = 1u << 0u,
    Register = 1u << 1u,
    Net = 1u << 2u,
    Variable = 1u << 3u,
    Control = 1u << 4u,
    Module = 1u << 5u,
    Assign = 1u << 6u
};

enum class NetOpType {
    Ignore,
    Add,
    Subtract,
    Ternary,
    Equal,
    LogicalNot,
    BinaryAnd,
    BinaryOr,
    BitwiseNot
};

enum class PortType { None, Input, Output };
enum class EventType { None, Posedge, Negedge };

inline NodeType operator|(NodeType a, NodeType b) {
    return static_cast<NodeType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline NodeType operator&(NodeType a, NodeType b) {
    return static_cast<NodeType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

enum class EdgeType : unsigned int {
    Blocking = 1u << 0u,
    NonBlocking = 1u << 1u,
    Slice = 1u << 2u,
    Control = 1u << 3u,
    True = Control | 1u << 4u,
    False = Control | 1u << 5u
};

inline EdgeType operator|(EdgeType a, EdgeType b) {
    return static_cast<EdgeType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline EdgeType operator&(EdgeType a, EdgeType b) {
    return static_cast<EdgeType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

struct Node;
struct Edge;
class FSMResult;

struct ModuleDefInfo {
    std::string name;
    std::unordered_map<std::string, const Node*> params;
};

struct Node {
public:
    uint64_t id;
    std::string name;
    NodeType type = NodeType::Net;

    std::vector<std::unique_ptr<Edge>> edges_to;
    std::unordered_set<Edge*> edges_from;

    Node* parent = nullptr;
    std::vector<Node*> children;

    // by default we don't care. only needed if it's a net and uses certain op
    NetOpType op = NetOpType::Ignore;

    // only used when it's a constant node
    int64_t value = 0;

    // for any wires/regs
    std::string wire_type;
    // only for the ports. other nodes will have none
    PortType port_type = PortType::None;
    EventType event_type = EventType::None;
    // only for module instances
    std::unique_ptr<ModuleDefInfo> module_def;
    // member access
    // for interface and packed struct
    std::unordered_map<std::string, Node*> members;
    // for module
    // number of unnamed gen block
    uint32_t num_gen_block = 0;

    Node(uint64_t id, std::string name) : id(id), name(std::move(name)) {}
    Node(uint64_t id, std::string name, Node* parent)
        : id(id), name(std::move(name)), parent(parent) {}
    Node(uint64_t id, std::string name, NodeType type)
        : id(id), name(std::move(name)), type(type) {}
    Node(uint64_t id, std::string name, NodeType type, Node* parent)
        : id(id), name(std::move(name)), type(type), parent(parent) {}
    template <typename... Args>
    void update(const std::string& n, Args... args) {
        name = n;
        update(std::forward<Args>(args)...);
    }

    template <typename... Args>
    void update(NodeType t, Args... args) {
        sink{args...};
        type = t;
    }
    template <typename... Args>
    void update(Node* p, Args... args) {
        sink{args...};
        parent = p;
    }

    template <typename... Args>
    Edge* add_edge(Node* to, Args... args) {
        auto edge = std::make_unique<Edge>(this, to, args...);
        auto e = edge.get();
        edges_to.emplace_back(std::move(edge)).get();
        to->edges_from.emplace(e);
        return e;
    }

    inline bool has_type(NodeType t) const { return static_cast<bool>(t & type); }

    [[nodiscard]] std::string handle_name() const;
    [[nodiscard]] std::string handle_name(const Node* parent) const;
    bool child_of(const Node* node) const;

private:
    static void update() {}
    struct sink {
        template <typename... Args>
        explicit sink(Args const&...) {}
    };
};

struct Edge {
public:
    Node* from;
    Node* to;
    EdgeType type = EdgeType::Blocking;

    Edge(Node* from, Node* to) : from(from), to(to) {}
    Edge(Node* from, Node* to, EdgeType type) : from(from), to(to), type(type) {}

    [[nodiscard]] inline bool has_type(EdgeType t) const { return (t & type) == t; }
    [[nodiscard]] inline bool is_assign() const {
        return type == EdgeType::Blocking || type == EdgeType::NonBlocking;
    }
};

class Graph {
public:
    template <typename... Args>
    inline Node* add_node(uint64_t key, Args... args) {
        // if we have a node already, we need to update the values
        Node* n;
        if (has_node(key)) {
            n = nodes_map_.at(key);
            n->update(args...);
        } else {
            auto ptr = std::make_unique<Node>(key, args...);
            n = ptr.get();
            nodes_.emplace_back(std::move(ptr)).get();
            nodes_map_.emplace(key, n);
        }
        if (n->parent) {
            n->parent->children.emplace_back(n);
        }
        return n;
    }
    inline void alias_node(uint64_t key, Node* node) { nodes_map_.emplace(key, node); }

    bool has_node(uint64_t key) const { return nodes_map_.find(key) != nodes_map_.end(); }
    inline void remove_node(uint64_t key) { nodes_map_.erase(key); }

    Node* get_node(uint64_t key);

    static bool has_path(const Node* from, const Node* to, uint64_t max_depth = 1u << 20u);
    static bool has_path(const Node* from, const Node* to,
                         const std::function<bool(const Edge*)>& cond);

    Node* select(const std::string& name);
    void identify_registers();
    [[nodiscard]] std::vector<Node*> get_registers() const;
    static bool constant_driver(const Node* node);

    static bool reachable(const Node* from, const Node* to);
    static bool has_loop(const Node* node);
    static bool has_control_loop(const Node* node);
    static std::vector<const Node*> find_sinks(const Node* node, uint32_t depth = 0);
    // notice that get_constant_source uses the same algorithm as constant_driver
    // in practice we can just use get_constant_source since if it's not constant driver
    // an empty set will be returned
    // the reason to return an edge is that in case of a counter, the constant is not directly
    // driving the state variable; rather, it drives a net (expression)
    [[nodiscard]] static std::unordered_set<const Edge*> get_constant_source(const Node* node);
    // given the output of get_constant_source, this function calculate if it is a counter type
    static bool is_counter(const Node* node, const std::unordered_set<const Edge*>& edges);
    static bool in_direct_assign_chain(const Node* from, const Node* to);
    static std::unordered_set<const Edge*> find_connection_cond(
        const Node* from, const std::function<bool(const Edge*)>& predicate);
    static std::unordered_set<const Edge*> find_connection_cond(
        const Node* from, const std::function<bool(const Edge*)>& predicate,
        const std::function<bool(const Edge*)>& terminate);
    static std::vector<const Node*> route(const Node* from, const Node* to,
                                          const std::function<bool(const Edge*)>& predicate,
                                          uint32_t depth = 0);

    std::vector<FSMResult> identify_fsms();
    std::vector<FSMResult> identify_fsms(const Node* top);
    static std::unordered_map<const Node*, std::unordered_set<const Node*>> group_fsms(
        const std::vector<FSMResult>& fsms, bool fast_mode = true);

    uint64_t get_free_id() { return free_id_ptr_--; }

    Node* copy_node(const Node* node, bool copy_connection = true);
    [[nodiscard]] const std::vector<std::unique_ptr<Node>>& nodes() const { return nodes_; }

private:
    std::unordered_map<uint64_t, Node*> nodes_map_;
    std::vector<std::unique_ptr<Node>> nodes_;

    // nodes search for cache
    std::vector<Node*> cache_nodes_;

    uint64_t free_id_ptr_ = 0xFFFFFFFFFFFFFFFF;
};

}  // namespace fsm
#endif  // PASTAFARIAN_GRAPH_HH
