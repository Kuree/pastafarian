module fsm8 (
    input logic clk,
    input logic rst,
    input logic in
);

typedef enum logic {
    STATE0 = 1'b0,
    STATE1 = 1'b1
} STATE;

STATE state0;
STATE next_state;
STATE state1;
STATE state2;

always_comb begin
    next_state = state0;
    if (state0 == STATE0) begin
        if (in) next_state = STATE1;
        else next_state = STATE0;
    end
    else if (state1 == STATE1) begin
        next_state = STATE0;
    end
    else if (state2 == STATE1) begin
        next_state = STATE1;
    end
end

always_ff @(posedge clk, posedge rst) begin
    if (rst) begin
        state0 <= STATE0;
        state1 <= STATE0;
        state2 <= STATE0;
    end else begin
        state0 <= next_state;
        state1 <= state0;
        state2 <= state1;
    end
end

endmodule
