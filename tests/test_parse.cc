#include <filesystem>
#include <memory>

#include "../src/parser.hh"
#include "gtest/gtest.h"
#include "util.hh"


TEST_F(ParserTest, hierarchy) {  // NOLINT
    parse("hierarchy.json");
    auto top_in = g.select("top.in");
    EXPECT_NE(top_in, nullptr);
    auto top_out = g.select("top.out");
    EXPECT_NE(top_out, nullptr);
    EXPECT_TRUE(g.has_path(top_in, top_out));
}

TEST_F(ParserTest, param) {  // NOLINT
    parse("param.json");
    auto in = g.select("mod.in");
    auto out = g.select("mod.out");
    EXPECT_TRUE(in && out);
    EXPECT_TRUE(g.has_path(in, out));

    auto local_param_value = g.select("mod.value");
    EXPECT_NE(local_param_value, nullptr);
    EXPECT_TRUE(g.has_path(local_param_value, out));
}

TEST_F(ParserTest, reg) {  // NOLINT
    parse("reg.json");
    auto rst = g.select("register.rst");
    auto out = g.select("register.out");
    EXPECT_NE(rst, nullptr);
    EXPECT_NE(out, nullptr);

    EXPECT_TRUE(g.has_path(rst, out));
}

TEST_F(ParserTest, case_) {  // NOLINT
    parse("case.json");
    auto in = g.select("switch_test.in");
    auto out = g.select("switch_test.out");
    EXPECT_NE(in, nullptr);
    EXPECT_NE(out, nullptr);

    EXPECT_TRUE(g.has_path(in, out));
}

TEST_F(ParserTest, enum_) {  // NOLINT
    parse("enum.json");
    auto in = g.select("mod.in");
    auto out = g.select("mod.out");
    EXPECT_NE(in, nullptr);
    EXPECT_NE(out, nullptr);

    EXPECT_TRUE(g.has_path(in, out));
}

TEST_F(ParserTest, op) {  // NOLINT
    parse("op.json");
    auto a = g.select("mod.a");
    auto b = g.select("mod.b");
    auto c = g.select("mod.c");
    auto e = g.select("mod.e");
    auto f = g.select("mod.f");
    auto h = g.select("mod.h");
    auto i = g.select("mod.i");

    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);
    EXPECT_NE(c, nullptr);
    EXPECT_NE(e, nullptr);
    EXPECT_NE(f, nullptr);
    EXPECT_NE(h, nullptr);
    EXPECT_NE(i, nullptr);

    EXPECT_TRUE(g.has_path(e, i));
    EXPECT_TRUE(g.has_path(a, c));
    EXPECT_TRUE(g.has_path(b, a));
    EXPECT_TRUE(g.has_path(f, b));
    EXPECT_TRUE(g.has_path(a, b));
    EXPECT_TRUE(g.has_path(c, b));
    EXPECT_TRUE(g.has_path(a, f));
}

TEST_F(ParserTest, fsm1) {  // NOLINT
    parse("fsm1.json");

    // should be able to select without the top
    auto next_state = g.select("Color_next_state");
    auto current_state = g.select("Color_current_state");
    auto in = g.select("in");
    EXPECT_NE(next_state, nullptr);
    EXPECT_NE(current_state, nullptr);
    EXPECT_NE(in, nullptr);

    EXPECT_TRUE(g.has_path(in, next_state));
    EXPECT_TRUE(g.has_path(next_state, current_state));
    EXPECT_TRUE(g.has_path(current_state, next_state));
}