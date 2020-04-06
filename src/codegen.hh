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

class Module {
public:
    std::string name;
    std::map<std::string, const Node *> ports;

    explicit Module(Graph *graph, const std::string &top_name = "");
    void set_fsm_result(const std::vector<FSMResult> &result) { fsm_results_ = result; }
    void create_properties();

private:
    const Node *root_module_;
    std::vector<FSMResult> fsm_results_;
    std::map<uint32_t, std::shared_ptr<Property>> properties_;
    std::string clk_name_;
    std::string reset_pin;

    void analyze_pins();
};

}  // namespace fsm

#endif  // PASTAFARIAN_CODEGEN_HH
