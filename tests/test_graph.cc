#include <memory>

#include "util.hh"

using fsm::Graph;

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

TEST_F(GraphTest, check_reachable) {    // NOLINT
    parse("const_driver.json");

    auto a = g.select("mod.a");
    auto b = g.select("mod.b");
    auto c = g.select("mod.c");
    auto d = g.select("mod.d");

    EXPECT_TRUE(Graph::has_loop(a));
    EXPECT_FALSE(Graph::has_loop(b));
    EXPECT_TRUE(Graph::reachable(a, b));
    EXPECT_TRUE(Graph::has_loop(c));
    EXPECT_TRUE(Graph::has_loop(d));
    EXPECT_FALSE(Graph::reachable(a, d));

    EXPECT_FALSE(Graph::has_control_loop(a));
    EXPECT_FALSE(Graph::has_control_loop(b));
    EXPECT_FALSE(Graph::has_control_loop(c));
    EXPECT_FALSE(Graph::has_control_loop(d));
}

TEST_F(GraphTest, check_control_loop) {    // NOLINT
    parse("fsm1.json");

    auto current_state = g.select("mod.Color_current_state");
    auto next_state = g.select("mod.Color_next_state");
    auto out = g.select("mod.out");
    EXPECT_TRUE(Graph::has_control_loop(current_state));
    EXPECT_TRUE(Graph::has_control_loop(next_state));
    EXPECT_FALSE(Graph::has_control_loop(out));
}

TEST_F(GraphTest, in_assign_chain) {    // NOLINT
    parse("const_driver.json");
    auto a = g.select("mod.a");
    auto b = g.select("mod.b");

    EXPECT_TRUE(Graph::in_direct_assign_chain(a, b));

}

TEST_F(GraphTest, const_source_values_fsm1) {    // NOLINT
    parse("fsm1.json");

    auto state = g.select("Color_current_state");
    EXPECT_NE(state, nullptr);

    auto state_values = Graph::get_constant_source(state);
    std::unordered_map<int64_t, std::string> names;
    for (auto edge: state_values) {
        auto n = edge->from;
        EXPECT_EQ(n->type, fsm::NodeType::Constant);
        names.emplace(n->value, n->name);
    }
    // no name conflicts
    EXPECT_EQ(names.size(), 2);
    EXPECT_EQ(names.at(1), "Red");

    EXPECT_FALSE(Graph::is_counter(state, state_values));
}

TEST_F(GraphTest, counter) {   // NOLINT
    parse("const_driver.json");

    auto a = g.select("mod.a");
    auto b = g.select("mod.b");
    auto f = g.select("mod.f");
    auto g_ = g.select("mod.g");

    {
        auto values = Graph::get_constant_source(a);
        EXPECT_FALSE(values.empty());
        EXPECT_TRUE(Graph::is_counter(a, values));
    }

    {
        auto values = Graph::get_constant_source(b);
        EXPECT_FALSE(values.empty());
        EXPECT_TRUE(Graph::is_counter(b, values));
    }

    {
        auto values = Graph::get_constant_source(f);
        EXPECT_FALSE(values.empty());
        EXPECT_FALSE(Graph::is_counter(f, values));
    }

    {
        auto values = Graph::get_constant_source(g_);
        EXPECT_FALSE(values.empty());
        EXPECT_FALSE(Graph::is_counter(g_, values));
    }
}