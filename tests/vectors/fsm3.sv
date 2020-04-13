module mod (
    input logic clk,
    input logic reset,
    input logic in,
    output logic out
);


logic [1:0] state;
logic [1:0] next_state;

always_ff @(posedge clk, posedge reset) begin
    if (reset) state <= 0;
    else state <= next_state;
end

always_comb begin
    if (state == 0) begin
        if (in) begin next_state = 1; end
        else begin next_state = 2; end
    end else if (state == 1) begin
        if (in) begin next_state = 2; end
        else begin next_state = 3; end
    end else if (state == 2) begin
        next_state = 3;
    end else begin
        next_state = 1;
    end
end

always_comb begin
    out = state != 0;
end

endmodule
