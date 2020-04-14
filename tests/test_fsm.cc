#include "util.hh"
#include "../src/fsm.hh"


TEST_F(GraphTest, fsm_extract_fsm1) {  // NOLINT
    parse("fsm1.json");
    auto fsms = g.identify_fsms();
    EXPECT_EQ(fsms.size(), 1);
    auto const &fsm = fsms[0];

    EXPECT_EQ(fsm.node()->name, "Color_current_state");

    auto states = fsm.unique_states();
    EXPECT_EQ(states.size(), 2);

    EXPECT_FALSE(fsm.is_counter());

    auto s_arcs = fsm.syntax_arc();
    EXPECT_EQ(s_arcs.size(), 4);
}

TEST_F(GraphTest, fsm_extract_fsm2) {  // NOLINT
    parse("fsm2.json");
    auto fsms = g.identify_fsms();
    EXPECT_EQ(fsms.size(), 1);
    auto const &fsm = fsms[0];

    EXPECT_EQ(fsm.node()->name, "state");

    auto states = fsm.unique_states();
    EXPECT_EQ(states.size(), 3);

    EXPECT_FALSE(fsm.is_counter());

    // CHISEL is not working yet
    auto s_arcs = fsm.syntax_arc();
    EXPECT_EQ(s_arcs.size(), 4);
}

TEST_F(GraphTest, fsm_extract_fsm3) {  // NOLINT
    parse("fsm3.json");
    auto fsms = g.identify_fsms();
    EXPECT_EQ(fsms.size(), 1);
    auto const &fsm = fsms[0];

    EXPECT_EQ(fsm.node()->name, "state");

    auto states = fsm.unique_states();
    EXPECT_EQ(states.size(), 4);

    EXPECT_FALSE(fsm.is_counter());

    auto syntax_arcs = fsm.syntax_arc();
    EXPECT_FALSE(syntax_arcs.empty());

    auto s_arcs = fsm.syntax_arc();
    EXPECT_EQ(s_arcs.size(), 5);
}