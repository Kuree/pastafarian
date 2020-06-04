#include "../src/fsm.hh"
#include "util.hh"

TEST_F(GraphTest, fsm_extract_fsm1) {  // NOLINT
    parse("fsm1.json");
    auto fsms = g.identify_fsms();
    fsm::merge_pipelined_fsm(fsms);

    EXPECT_EQ(fsms.size(), 1);
    auto &fsm = fsms[0];

    EXPECT_EQ(fsm.node()->name, "Color_current_state");

    auto states = fsm.unique_states();
    EXPECT_EQ(states.size(), 2);

    EXPECT_FALSE(fsm.is_counter());

    fsm.extract_fsm_arcs();
    const auto& s_arcs = fsm.syntax_arc();
    EXPECT_EQ(s_arcs.size(), 4);
}

TEST_F(GraphTest, fsm_extract_fsm2) {  // NOLINT
    parse("fsm2.json");
    auto fsms = g.identify_fsms();
    fsm::merge_pipelined_fsm(fsms);

    EXPECT_EQ(fsms.size(), 1);
    auto &fsm = fsms[0];

    EXPECT_EQ(fsm.node()->name, "state");

    auto states = fsm.unique_states();
    EXPECT_EQ(states.size(), 3);

    EXPECT_FALSE(fsm.is_counter());

    fsm.extract_fsm_arcs();
    const auto& s_arcs = fsm.syntax_arc();
    EXPECT_EQ(s_arcs.size(), 4);
}

TEST_F(GraphTest, fsm_extract_fsm3) {  // NOLINT
    parse("fsm3.json");
    auto fsms = g.identify_fsms();
    fsm::merge_pipelined_fsm(fsms);


    EXPECT_EQ(fsms.size(), 1);
    auto &fsm = fsms[0];

    EXPECT_EQ(fsm.node()->name, "state");

    auto states = fsm.unique_states();
    EXPECT_EQ(states.size(), 4);

    EXPECT_FALSE(fsm.is_counter());

    fsm.extract_fsm_arcs();
    const auto& s_arcs = fsm.syntax_arc();
    EXPECT_EQ(s_arcs.size(), 5);
}

TEST_F(GraphTest, fsm_extract_fsm4) {  // NOLINT
    parse("fsm4.json");
    auto fsms = g.identify_fsms();
    fsm::merge_pipelined_fsm(fsms);

    EXPECT_EQ(fsms.size(), 1);
    auto &fsm = fsms[0];

    EXPECT_EQ(fsm.node()->name, "release_state");

    auto states = fsm.unique_states();
    // NOTICE: rocket chip DCache has dead code (unreachable state) when the test is generated
    //   i.e. release_state <= s_probe_retry;
    EXPECT_EQ(states.size(), 7);
    auto const_comp_states = fsm.comp_const();
    // 0 is never compared against
    EXPECT_EQ(const_comp_states.size(), 7);

    EXPECT_FALSE(fsm.is_counter());

    fsm.extract_fsm_arcs();
    const auto& s_arcs = fsm.syntax_arc();
    // NOTICE: this high number is due to dead code generation. In scala there aren't as many
    //  state transition as in verilog.
    EXPECT_EQ(s_arcs.size(), 47);
}

TEST_F(GraphTest, fsm_extract_fsm5) {  // NOLINT
    parse("fsm5.json");
    auto fsms = g.identify_fsms();
    fsm::merge_pipelined_fsm(fsms);

    EXPECT_EQ(fsms.size(), 1);
    auto const &fsm = fsms[0];

    EXPECT_EQ(fsm.node()->name, "r_w_seq_current_state");

    auto states = fsm.unique_states();
    EXPECT_EQ(states.size(), 3);

    EXPECT_FALSE(fsm.is_counter());
}

TEST_F(GraphTest, fsm_extract_fsm6) {  // NOLINT
    parse("fsm6.json");

    auto fsms = g.identify_fsms();
    fsm::merge_pipelined_fsm(fsms);

    EXPECT_EQ(fsms.size(), 1);
    auto &fsm = fsms[0];
    fsm.extract_fsm_arcs();
    auto syntax_arc = fsm.syntax_arc();

    // we know that off can only go to idle based on the syntax
    uint32_t count = 0;
    for (auto const &[from, to] : syntax_arc) {
        if (from->value == 0) {
            EXPECT_EQ(from->name, "OFF");
            count += 1;
            EXPECT_EQ(to->value, 1);
            EXPECT_EQ(to->name, "IDLE");
        }
    }
    EXPECT_EQ(count, 1);
}

TEST_F(GraphTest, fsm_extract_fsm7) {  // NOLINT
    parse("fsm7.json");

    auto fsms = g.identify_fsms();
    fsm::merge_pipelined_fsm(fsms);

    EXPECT_EQ(fsms.size(), 2);

    auto grouped_fsm = fsm::Graph::group_fsms(fsms);
    EXPECT_EQ(grouped_fsm.size(), 1);
}

TEST_F(GraphTest, fsm_extract_fsm8) {  // NOLINT
    parse("fsm8.json");

    auto fsms = g.identify_fsms();
    EXPECT_EQ(fsms.size(), 3);
    fsm::identify_fsm_arcs(fsms);

    fsm::merge_pipelined_fsm(fsms);

    EXPECT_EQ(fsms.size(), 1);

    auto fsm = fsms[0];
    auto const &syntax_arc = fsm.syntax_arc();
    EXPECT_EQ(syntax_arc.size(), 4);
}