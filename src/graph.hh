#ifndef PASTAFARIAN_GRAPH_HH
#define PASTAFARIAN_GRAPH_HH

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class NodeType {
    Constant = 1u << 0u,
    Register = 1u << 1u,
    Net = 1u << 2u,
    Variable = 1u << 3u,
    Control = 1u << 4u,
    Module = 1u << 5u,
    Assign = 1u << 6u
};

inline NodeType operator|(NodeType a, NodeType b) {
    return static_cast<NodeType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline NodeType operator&(NodeType a, NodeType b) {
    return static_cast<NodeType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

enum class EdgeType { Blocking, NonBlocking, Slice };

struct Edge;

struct Node {
public:
    uint64_t id;
    std::string name;
    NodeType type = NodeType::Net;

    std::vector<std::unique_ptr<Edge>> edges_to;
    std::unordered_set<Edge*> edges_from;

    Node* parent = nullptr;
    std::vector<Node*> children;

    // only used when it's a constant node
    int64_t value = 0;

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

private:
    static void update() {}
    struct sink {
        template <typename... Args>
        sink(Args const&...) {}
    };
};

struct Edge {
public:
    Node* from;
    Node* to;
    EdgeType type = EdgeType::Blocking;

    Edge(Node* from, Node* to) : from(from), to(to) {}
    Edge(Node* from, Node* to, EdgeType type) : from(from), to(to), type(type) {}
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

    Node* get_node(uint64_t key);

    static bool has_path(Node* from, Node* to, uint64_t max_depth = 1u << 20u);

    Node* select(const std::string& name);
    void identify_registers();
    [[nodiscard]] std::vector<Node*> get_registers() const;
    static bool constant_driver(Node* node);

    static bool reachable(const Node* from, const Node *to);
    static bool has_loop(const Node *node);
    static bool has_control_loop(const Node *node);
    [[nodiscard]]
    static std::unordered_set<const Node*> get_constant_source(const Node *node);

    uint64_t get_free_id() { return free_id_ptr_--; }

private:
    std::unordered_map<uint64_t, Node*> nodes_map_;
    std::vector<std::unique_ptr<Node>> nodes_;

    // nodes search for cache
    std::vector<Node*> cache_nodes_;

    uint64_t free_id_ptr_ = 0xFFFFFFFFFFFFFFFF;
};

#endif  // PASTAFARIAN_GRAPH_HH
