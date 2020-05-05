#ifndef PASTAFARIAN_CODEGEN_HH
#define PASTAFARIAN_CODEGEN_HH
#include <map>

#include "graph.hh"
#include "source.hh"
#include "util.hh"

namespace fsm {

constexpr char TOP_NAME[] = "TOP";
constexpr char PROPERTY_LABEL_PREFIX[] = "FSM_STATE_";

enum class ResetType { Default, None, Posedge, Negedge };

class Property {
public:
    uint32_t id;
    const Node *top;
    const Node *state_var1;
    const Node *state_value1;
    const Node *state_var2 = nullptr;
    const Node *state_value2 = nullptr;
    uint32_t delay = 0;
    std::string clk_name;
    // whether it's a valid property, which will be determined by mail
    bool valid = false;
    bool should_be_valid = false;

    Property(uint32_t id, const Node *top, std::string clk_name, const Node *state_var1,
             const Node *state_value1);
    Property(uint32_t id, const Node *top, std::string clk_name, const Node *state_var1,
             const Node *state_value1, const Node *state_var2, const Node *state_value2);

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
    void add_cross_properties(
        const std::unordered_map<const Node *, std::unordered_set<const Node *>> &groups);
    [[nodiscard]] Property &get_property(uint32_t id) const;
    [[nodiscard]] Property *get_property(const Node *node, uint32_t state_value) const;
    [[nodiscard]] Property *get_property(const Node *node, uint32_t state_from,
                                         uint32_t state_to) const;
    [[nodiscard]] std::vector<const Property *> get_property(const Node *node) const;
    [[nodiscard]] bool has_property(const Node *node) const;
    [[nodiscard]] std::vector<const Property *> get_property(const Node *node1,
                                                             const Node *node2) const;

    void set_reset_name(const std::string &reset_name) { reset_name_ = reset_name; }
    void set_clock_name(const std::string &clock_name) { clock_name_ = clock_name; }
    void set_reset_type(ResetType value) { reset_type_ = value; }
    void set_double_edge_clock(bool value) { double_edge_clock_ = value; }
    [[nodiscard]] const std::string &clock_name() const { return clock_name_; }
    [[nodiscard]] const std::string &reset_name() const { return reset_name_; }
    [[nodiscard]] ResetType reset_type() const { return reset_type_; }
    [[nodiscard]] const SourceManager &parser_result() const { return parser_result_; }
    [[nodiscard]] bool double_edge_clock() const { return double_edge_clock_; }
    void analyze_pins();

    [[nodiscard]] inline const Node *top() const { return root_module_; }

    [[nodiscard]] std::string str() const;
    void to_file(const std::string &filename) const;

    void set_param_values(const std::unordered_map<std::string, int64_t> &params);

private:
    SourceManager parser_result_;
    const Node *root_module_;
    std::vector<FSMResult> fsm_results_;
    std::map<uint32_t, std::shared_ptr<Property>> properties_;
    std::string clock_name_;
    std::string reset_name_;
    ResetType reset_type_ = ResetType::Default;
    bool double_edge_clock_;
    std::unordered_map<std::string, int64_t> param_values_;

    void analyze_reset();
};

class FormalGeneration {
public:
    explicit FormalGeneration(VerilogModule &module) : module_(module) {}
    void run();
    [[nodiscard]] virtual bool has_tools() const = 0;

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
    [[nodiscard]] bool has_tools() const override;
    static bool has_jaspergold();

    void set_timeout_limit(uint32_t value) { timeout_limit_ = value; }

private:
    void create_command_file(const std::string &cmd_filename, const std::string &wrapper_filename);
    void run_process() override;
    void parse_result() override;
    [[nodiscard]] static std::string jg_working_dir();

    uint32_t timeout_limit_ = 0;
};

}  // namespace fsm

#endif  // PASTAFARIAN_CODEGEN_HH
