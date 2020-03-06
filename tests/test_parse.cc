#include "gtest/gtest.h"
#include <memory>
#include <fstream>
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
}

TEST_F(ParserTest, reg) {   // NOLINT
    parse("reg.json");
    auto rst = g.select("register.rst");
    auto out = g.select("register.out");
    EXPECT_NE(rst, nullptr);
    EXPECT_NE(out, nullptr);

    EXPECT_TRUE(g.has_path(rst, out));
}