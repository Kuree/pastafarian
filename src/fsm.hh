#ifndef PASTAFARIAN_FSM_HH
#define PASTAFARIAN_FSM_HH

#include <set>

#include "graph.hh"

namespace fsm {

class FSMResult {
public:
    FSMResult(const Node *node, const std::unordered_set<const Edge *> &const_src)
        : node_(node), const_src_(const_src) {}

    inline const Node *node() const { return node_; }
    inline const std::unordered_set<const Edge *> &const_src() const { return const_src_; }

    [[nodiscard]] std::set<std::pair<const Node *, const Node *>> self_arc() const;

    FSMResult() = delete;

private:
    const Node *node_;
    std::unordered_set<const Edge *> const_src_;
};

};  // namespace fsm

#endif  // PASTAFARIAN_FSM_HH
