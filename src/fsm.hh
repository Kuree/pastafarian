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
    [[nodiscard]] std::unordered_set<const Node *> comp_const() const;

    // get state transition arcs from heuristics
    [[nodiscard]] const std::set<std::pair<const Node *, const Node *>> &syntax_arc() const {
        return syntax_arc_;
    }
    void extract_fsm_arcs();

    [[nodiscard]] std::vector<const Node *> unique_states() const;

    [[nodiscard]] std::unordered_set<const Node *> counter_values() const;

    FSMResult() = delete;

private:
    const Node *node_;
    std::unordered_set<const Edge *> const_src_;
    bool is_counter_;

    static Node *get_const_from_comp(const Node *node_comp);

    // store extracted syntax arc
    // used when merging fsm as well
    std::set<std::pair<const Node *, const Node *>> syntax_arc_;
};

void identify_fsm_arcs(std::vector<FSMResult> &fsm_result);
void merge_pipelined_fsm(std::vector<FSMResult> &fsm_result);

};  // namespace fsm

#endif  // PASTAFARIAN_FSM_HH
