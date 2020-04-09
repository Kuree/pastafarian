#pragma once

#include "../src/parser.hh"
#include "gtest/gtest.h"
#include <filesystem>

class LoadFile : public ::testing::Test {
public:
    void SetUp() override { p = std::make_unique<fsm::Parser>(&g); }

    void parse(const std::string &filename) {
        EXPECT_TRUE(std::filesystem::exists(filename));
        p->parse(filename);
    }

    fsm::Graph g;
    std::unique_ptr<fsm::Parser> p;
};

class ParserTest : public LoadFile {};
class GraphTest : public LoadFile {};