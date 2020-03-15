#include <memory>

#include "util.hh"

TEST_F(GraphTest, get_registers_fsm1) {  // NOLINT
    parse("fsm1.json");
    g.identify_registers();
    auto regs = g.get_registers();
    EXPECT_FALSE(regs.empty());
    // we should have one registers here
    // Color_current_state
    EXPECT_EQ(regs.size(), 1);
}

TEST_F(GraphTest, check_const_driver_fsm) {  // NOLINT
    parse("fsm1.json");
    g.identify_registers();

    auto regs = g.get_registers();
    auto reg = regs.front();
    auto r = Graph::constant_driver(reg);
    EXPECT_TRUE(r);

    auto next_state = g.select("mod.Color_next_state");
    EXPECT_NE(next_state, nullptr);
    r = Graph::constant_driver(next_state);
    EXPECT_TRUE(r);

    // out should be constant driver as well
    // even thought it is not a registers

    auto out = g.select("mod.out");
    EXPECT_NE(out, nullptr);
    r = Graph::constant_driver(out);
    EXPECT_TRUE(r);
}

TEST_F(GraphTest, check_const_driver) {  // NOLINT
    parse("const_driver.json");

    auto a = g.select("mod.a");
    auto b = g.select("mod.b");
    auto c = g.select("mod.c");
    auto d = g.select("mod.d");
    auto e = g.select("mod.e");
    auto f = g.select("mod.f");
    auto g_ = g.select("mod.g");

    auto vars = {a, b, c, d, e, f, g_};
    for (auto const v: vars) {
        EXPECT_NE(v, nullptr);
    }

    EXPECT_TRUE(Graph::constant_driver(a));
    EXPECT_TRUE(Graph::constant_driver(b));
    EXPECT_FALSE(Graph::constant_driver(c));
    EXPECT_FALSE(Graph::constant_driver(d));
    EXPECT_FALSE(Graph::constant_driver(e));
    EXPECT_TRUE(Graph::constant_driver(f));
    EXPECT_TRUE(Graph::constant_driver(g_));
}