#ifndef PASTAFARIAN_FSM_HH
#define PASTAFARIAN_FSM_HH

#include <set>

#include "graph.hh"

namespace fsm {

class FSMResult {
public:
    FSMResult(const Node *node, std::unordered_set<const Edge *> const_src);

    [[nodiscard]] inline const Node *node() const { return node_; }
    [[nodiscard]] inline const std::unordered_set<const Edge *> &const_src() const {
        return const_src_;
    }
    [[nodiscard]] inline bool is_counter() const { return is_counter_; }

    [[nodiscard]] std::set<std::pair<const Node *, const Node *>> self_arc() const;

    [[nodiscard]] std::unordered_set<const Node*> unique_states() const;

    FSMResult() = delete;

private:
    const Node *node_;
    std::unordered_set<const Edge *> const_src_;
    bool is_counter_;
};

};  // namespace fsm

#endif  // PASTAFARIAN_FSM_HH
