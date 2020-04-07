#include "../src/codegen.hh"
#include "../src/fsm.hh"
#include "util.hh"

TEST_F(GraphTest, fsm_codegen1) {  // NOLINT
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
    EXPECT_TRUE(m.posedge_reset());
    EXPECT_NE(result.find(
                  "@(posedge clk) mod.Color_current_state == 1 |=> mod.Color_current_state == 1;"),
              std::string::npos);
}