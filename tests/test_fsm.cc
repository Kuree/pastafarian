#include "util.hh"
#include "../src/fsm.hh"


TEST_F(GraphTest, fsm_extract_fsm1) {  // NOLINT
    parse("fsm1.json");
    auto fsms = g.identify_fsms();
    EXPECT_EQ(fsms.size(), 1);
    auto const &fsm = fsms[0];

    EXPECT_EQ(fsm.node()->name, "Color_current_state");

    auto arcs = fsm.self_arc();
    // there are four state transitions
    // Red -> Red
    // Red -> Blue
    // Blue -> Blue
    // Blue ->Red
    EXPECT_EQ(arcs.size(), 4);

    EXPECT_FALSE(fsm.is_counter());
}

TEST_F(GraphTest, fsm_extract_fsm2) {  // NOLINT
    parse("fsm2.json");
    auto fsms = g.identify_fsms();
    EXPECT_EQ(fsms.size(), 1);
    auto const &fsm = fsms[0];

    EXPECT_EQ(fsm.node()->name, "state");

    auto arcs = fsm.self_arc();
    // this is count two logic and implemented as an explicit FSM in chisel
    // 0 -> 0
    // 0 -> 1
    // 1 -> 2
    // 1 -> 0
    // 2 -> 2
    // 2 -> 0
    EXPECT_EQ(arcs.size(), 6);

    EXPECT_FALSE(fsm.is_counter());
}