#include "util.hh"
#include "../src/fsm.hh"


TEST_F(GraphTest, fsm_extract_fsm1) {  // NOLINT
    parse("fsm1.json");
    auto fsms = g.identify_fsms();
    EXPECT_EQ(fsms.size(), 1);
    auto const &fsm = fsms[0];

    EXPECT_EQ(fsm.node()->name, "Color_current_state");

    auto arcs = fsm.self_arc();
    EXPECT_EQ(arcs.size(), 2);
}