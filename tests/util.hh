#pragma once

#include "../src/parser.hh"
#include "gtest/gtest.h"
#include <filesystem>

class LoadFile : public ::testing::Test {
public:
    void SetUp() override { p = std::make_unique<Parser>(&g); }

    void parse(const std::string &json_filename) {
        EXPECT_TRUE(std::filesystem::exists(json_filename));
        p->parse(json_filename);
    }

    Graph g;
    std::unique_ptr<Parser> p;
};

class ParserTest : public LoadFile {};
class GraphTest : public LoadFile {};