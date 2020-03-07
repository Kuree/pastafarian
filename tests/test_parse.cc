#include "gtest/gtest.h"
#include <memory>
#include <filesystem>
#include "../src/parser.hh"

class ParserTest: public ::testing::Test {
protected:
    void SetUp() override {
        p = std::make_unique<Parser>(&g);
    }

    void parse(const std::string &json_filename) {
        EXPECT_TRUE(std::filesystem::exists(json_filename));
        p->parse(json_filename);
    }

    Graph g;
    std::unique_ptr<Parser> p;
};

TEST_F(ParserTest, hierarchy) {   // NOLINT
    parse("hierarchy.json");
    auto top_in = g.select("top.in");
    EXPECT_NE(top_in, nullptr);
    auto top_out = g.select("top.out");
    EXPECT_NE(top_out, nullptr);
    EXPECT_TRUE(g.has_path(top_in, top_out));
}

TEST_F(ParserTest, param) {   // NOLINT
    parse("param.json");
    auto in = g.select("mod.in");
    auto out = g.select("mod.out");
    EXPECT_TRUE(in && out);
    EXPECT_TRUE(g.has_path(in, out));

    auto local_param_value = g.select("mod.value");
    EXPECT_NE(local_param_value, nullptr);
    EXPECT_TRUE(g.has_path(local_param_value, out));
}

TEST_F(ParserTest, reg) {   // NOLINT
    parse("reg.json");
    auto rst = g.select("register.rst");
    auto out = g.select("register.out");
    EXPECT_NE(rst, nullptr);
    EXPECT_NE(out, nullptr);

    EXPECT_TRUE(g.has_path(rst, out));
}

TEST_F(ParserTest, case_) {   // NOLINT
    parse("case.json");
    auto in = g.select("switch_test.in");
    auto out = g.select("switch_test.out");
    EXPECT_NE(in, nullptr);
    EXPECT_NE(out, nullptr);

    EXPECT_TRUE(g.has_path(in, out));
}


TEST_F(ParserTest, enum_) {   // NOLINT
    parse("enum.json");
    auto in = g.select("mod.in");
    auto out = g.select("mod.out");
    EXPECT_NE(in, nullptr);
    EXPECT_NE(out, nullptr);

    EXPECT_TRUE(g.has_path(in, out));
}

TEST_F(ParserTest, op) {   // NOLINT
    parse("op.json");
    auto a = g.select("mod.a");
    auto b = g.select("mod.b");
    auto c = g.select("mod.c");
    auto e = g.select("mod.e");
    auto f = g.select("mod.f");

    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);
    EXPECT_NE(c, nullptr);
    EXPECT_NE(e, nullptr);
    EXPECT_NE(f, nullptr);

    EXPECT_TRUE(g.has_path(a, c));
    EXPECT_TRUE(g.has_path(b, a));
    EXPECT_TRUE(g.has_path(f, b));
    EXPECT_TRUE(g.has_path(a, b));
    EXPECT_TRUE(g.has_path(c, b));
}