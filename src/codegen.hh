#ifndef PASTAFARIAN_CODEGEN_HH
#define PASTAFARIAN_CODEGEN_HH
#include <map>
#include <optional>

#include "graph.hh"
#include "source.hh"
#include "util.hh"

namespace fsm {

constexpr char TOP_NAME[] = "TOP";
constexpr char PROPERTY_LABEL_PREFIX[] = "FSM_STATE_";

class Property {
public:
    uint32_t id;
    const Node *state_var1;
    const Node *state_value1;
    const Node *state_var2 = nullptr;
    const Node *state_value2 = nullptr;
    uint32_t delay = 0;
    std::string clk_name;
    // whether it's a valid property, which will be determined by mail
    bool valid = false;

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

    VerilogModule(Graph *graph, SourceManager parser_result, const std::string &top_name = "");
    void set_fsm_result(const std::vector<FSMResult> &result) { fsm_results_ = result; }
    void create_properties();
    Property &get_property(uint32_t id);

    void set_reset_name(const std::string &reset_name) { reset_name_ = reset_name; }
    void set_clock_name(const std::string &clock_name) { clock_name_ = clock_name; }
    void set_posedge_reset(bool value) { posedge_reset_ = value; }
    [[nodiscard]] const std::string &clock_name() const { return clock_name_; }
    [[nodiscard]] const std::string &reset_name() const { return reset_name_; }
    [[nodiscard]] bool posedge_reset() const { return posedge_reset_ ? *posedge_reset_ : true; }
    [[nodiscard]] const SourceManager &parser_result() const { return parser_result_; }
    void analyze_pins();

    [[nodiscard]] std::string str() const;

private:
    SourceManager parser_result_;
    const Node *root_module_;
    std::vector<FSMResult> fsm_results_;
    std::map<uint32_t, std::shared_ptr<Property>> properties_;
    std::string clock_name_;
    std::string reset_name_;
    std::optional<bool> posedge_reset_;

    void analyze_reset();
};

class FormalGeneration {
public:
    explicit FormalGeneration(VerilogModule &module) : module_(module) {}
    void run();

    virtual ~FormalGeneration() = default;

protected:
    VerilogModule &module_;

    virtual void parse_result() = 0;
    virtual void run_process() = 0;
};

class JasperGoldGeneration : public FormalGeneration {
public:
    explicit JasperGoldGeneration(VerilogModule &module) : FormalGeneration(module) {}
    void parse_result(const std::string &log_file);

private:
    void create_command_file(const std::string &filename);
    void run_process() override;
    void parse_result() override;
    [[nodiscard]] static std::string jg_working_dir();
};

}  // namespace fsm

#endif  // PASTAFARIAN_CODEGEN_HH
