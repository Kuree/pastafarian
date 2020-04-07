#ifndef PASTAFARIAN_CODEGEN_HH
#define PASTAFARIAN_CODEGEN_HH
#include <map>

#include "graph.hh"

namespace fsm {

constexpr char TOP_NAME[] = "TOP";

class Property {
public:
    uint32_t id;
    const Node *state_var1;
    const Node *state_value1;
    const Node *state_var2 = nullptr;
    const Node *state_value2 = nullptr;
    uint32_t delay = 0;
    std::string clk_name;

    Property(uint32_t id, std::string clk_name, const Node *state_var1, const Node *state_value1);
    Property(uint32_t id, std::string clk_name, const Node *state_var1, const Node *state_value1,
             const Node *state_var2, const Node *state_value2);

    [[nodiscard]] std::string str() const;
    [[nodiscard]] std::string property_name() const;
    [[nodiscard]] std::string property_label() const;
};

class VerilogModule {
public:
    std::string name;
    std::map<std::string, const Node *> ports;

    explicit VerilogModule(Graph *graph, const std::string &top_name = "");
    void set_fsm_result(const std::vector<FSMResult> &result) { fsm_results_ = result; }
    void create_properties();

    void set_reset_name(const std::string &reset_name) { reset_name_ = reset_name; }
    void set_clock_name(const std::string &clock_name) { clock_name_ = clock_name; }
    [[nodiscard]] const std::string &clock_name() const { return clock_name_; }
    [[nodiscard]] const std::string &reset_name() const { return reset_name_; }
    void analyze_pins();

    [[nodiscard]] std::string str() const;

private:
    const Node *root_module_;
    std::vector<FSMResult> fsm_results_;
    std::map<uint32_t, std::shared_ptr<Property>> properties_;
    std::string clock_name_;
    std::string reset_name_;
    bool posedge_reset = true;

};

}  // namespace fsm

#endif  // PASTAFARIAN_CODEGEN_HH
