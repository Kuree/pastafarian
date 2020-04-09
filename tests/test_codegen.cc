#include "../src/codegen.hh"
#include "../src/fsm.hh"
#include "util.hh"

TEST_F(GraphTest, fsm1_codegen) {  // NOLINT
    // skip the rest if can't find jaspergold in the env
    if (!fsm::JasperGoldGeneration::has_jaspergold()) {
        GTEST_SKIP_("jaspergold not available");
    }

    parse("fsm1.sv");
    auto fsms = g.identify_fsms();
    fsm::VerilogModule m(&g, p->parser_result());
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

    fsm::JasperGoldGeneration jg(m);

    jg.run();

    // test properties
    // this is just a sanity check
    auto red_prop = m.get_property(g.select("Color_current_state"), 0u);
    EXPECT_NE(red_prop, nullptr);
    EXPECT_TRUE(red_prop->valid);
}

TEST_F(GraphTest, fsm1_codegen_parse) { // NOLINT
    parse("fsm1.json");
    auto fsms = g.identify_fsms();
    fsm::VerilogModule m(&g, p->parser_result());
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

    fsm::JasperGoldGeneration jg(m);

    jg.parse_result("jg_session.log");
    // test properties
    auto state_var = g.select("Color_current_state");
    constexpr uint32_t RED = 0;
    constexpr uint32_t BLUE = 1;
    auto red_prop = m.get_property(state_var, RED);
    EXPECT_NE(red_prop, nullptr);
    EXPECT_TRUE(red_prop->valid);
    auto blue_prop = m.get_property(state_var, BLUE);
    EXPECT_NE(blue_prop, nullptr);
    EXPECT_TRUE(blue_prop->valid);
    auto red_blue  = m.get_property(state_var, RED, BLUE);
    EXPECT_NE(red_blue, nullptr);
    EXPECT_TRUE(red_blue->valid);
}