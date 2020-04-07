#include "util.hh"
#include "../src/fsm.hh"
#include "../src/codegen.hh"

TEST_F(GraphTest, fsm_codegen1) {   // NOLINT
    parse("fsm1.json");
    auto fsms = g.identify_fsms();
    fsm::VerilogModule m(&g);
    m.set_fsm_result(fsms);
    m.analyze_pins();
    m.create_properties();

    auto result = m.str();

    // clk, rst, in, out
    EXPECT_EQ(m.ports.size(), 4);
    EXPECT_FALSE(result.empty());
    printf("%s\n", result.c_str());
}