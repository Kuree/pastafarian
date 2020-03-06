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