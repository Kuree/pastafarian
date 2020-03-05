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
        std::ifstream f(json_filename);
        std::stringstream buffer;
        buffer << f.rdbuf();
        auto const &content = buffer.str();
        p->parse(content);
    }

    Graph g;
    std::unique_ptr<Parser> p;
};

TEST_F(ParserTest, hierarchy) {   // NOLINT
    parse("hierarchy.json");
}