typedef struct packed {
  logic [3:0] a;
  logic [1:0] b;
} s1;

module mod (
    input s1 in,
    input logic clk
);

s1 value1;
s1 value2;
s1 value3;

assign value1 = in;
assign value2.a = in.a;

always_ff @(posedge clk) begin
    value3 <= in;
end

always_comb begin
    value2.b = in.b;
end

endmodule
