#ifndef PASTAFARIAN_GRAPH_HH
#define PASTAFARIAN_GRAPH_HH

#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class NodeType { Constant, Register, Net, Control, Module };
enum class EdgeType { Blocking, NonBlocking, EdgeAssign };

struct Edge;
void update_edge_type(Edge* edge, EdgeType type);

struct Node {
public:
    uint64_t id;
    std::string name;
    NodeType type = NodeType::Net;

    std::vector<Edge> edges_to;
    std::unordered_set<Edge*> edges_from;

    Node* parent = nullptr;

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
        auto e = &edges_to.emplace_back(Edge(this, to, args...));
        if (type == NodeType::Constant) {
            update_edge_type(e, EdgeType::EdgeAssign);
        }
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

    Node* attr = nullptr;

    Edge(Node* from, Node* to) : from(from), to(to) {}
    Edge(Node* from, Node* to, EdgeType type, Node* attr)
        : from(from), to(to), type(type), attr(attr) {}
};
void inline update_edge_type(Edge* edge, EdgeType type) { edge->type = type; }

class Graph {
public:
    template <typename... Args>
    inline Node* add_node(uint64_t key, Args... args) {
        // if we have a node already, we need to update the values
        if (has_node(key)) {
            auto n = nodes_map_.at(key);
            n->update(args...);
            return n;
        } else {
            auto n = &nodes_.emplace_back(Node(key, args...));
            nodes_map_.emplace(key, n);
            return n;
        }
    }
    inline void alias_node(uint64_t key, Node* node) { nodes_map_.emplace(key, node); }
    bool has_node(uint64_t key) { return nodes_map_.find(key) != nodes_map_.end(); }
    Node* get_node(uint64_t key);

private:
    std::unordered_map<uint64_t, Node*> nodes_map_;
    std::vector<Node> nodes_;
};

#endif  // PASTAFARIAN_GRAPH_HH
