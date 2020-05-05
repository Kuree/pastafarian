module mod (
    input logic in,
    input logic clk,
    input logic reset
);

typedef enum logic {
    A = 0,
    B = 1
} FSM1;

typedef enum logic {
    C = 0,
    D = 1
} FSM2;

FSM1 state1, next_state1;
FSM2 state2, next_state2;

always_ff @(posedge clk, posedge reset) begin
    if (reset) begin
        state1 <= A;
    end else begin
        state1 <= next_state1;
    end
end

always_ff @(posedge clk, posedge reset) begin
    if (reset) begin
        state2 <= C;
    end else begin
        state2 <= next_state2;
    end
end

always_comb begin
    next_state1 = state1;
    if (in) begin 
        if (state1 == A) next_state1 = B;
        else next_state1 = A;
    end
end

always_comb begin
    next_state2 = state2;
    if (in && state1 == A) begin
        if (state2 == C) next_state2 = D;
    end else begin
        next_state2 = C;
    end

end


endmodule
