#include "gtest/gtest.h"

#include "../src/parser.hh"


TEST(parse, passthrough) {   // NOLINT
    Graph g;
    Parser p(&g);
    p.parse("PassThrough.sv", "PassThrough");
}