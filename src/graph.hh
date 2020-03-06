#ifndef PASTAFARIAN_GRAPH_HH
#define PASTAFARIAN_GRAPH_HH

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>

enum class NodeType { Constant, Register, Net, Control, Module, Assign };
enum class EdgeType { Blocking, NonBlocking };

struct Edge;

struct Node {
public:
    uint64_t id;
    std::string name;
    NodeType type = NodeType::Net;

    std::vector<std::unique_ptr<Edge>> edges_to;
    std::unordered_set<Edge*> edges_from;

    Node* parent = nullptr;
    std::unordered_set<Node*> children;

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
        type = t;
    }
    template <typename... Args>
    void update(Node* p, Args... args) {
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

private:
    static void update() {}
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
            n->parent->children.emplace(n);
        }
        return n;
    }
    inline void alias_node(uint64_t key, Node* node) { nodes_map_.emplace(key, node); }

    bool has_node(uint64_t key) { return nodes_map_.find(key) != nodes_map_.end(); }

    Node* get_node(uint64_t key);

    static bool has_path(Node* from, Node* to, uint64_t max_depth = 1u << 20u);

    Node *select(const std::string &name);

    uint64_t get_free_id() { return free_id_ptr_--; }

private:
    std::unordered_map<uint64_t, Node*> nodes_map_;
    std::vector<std::unique_ptr<Node>> nodes_;

    uint64_t free_id_ptr_ = 0xFFFFFFFFFFFFFFFF;
};

#endif  // PASTAFARIAN_GRAPH_HH
