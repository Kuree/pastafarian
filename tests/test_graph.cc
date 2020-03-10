#include <filesystem>
#include <memory>

#include "util.hh"

TEST_F(GraphTest, get_registers_fsm1) {    // NOLINT
    parse("fsm1.json");
    g.identify_registers();
    auto regs = g.get_registers();
    EXPECT_FALSE(regs.empty());
}