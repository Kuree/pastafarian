typedef struct packed {
  logic [3:0] a;
  logic [1:0] b;
} s1;

typedef struct packed {
    logic [3:0] c;
    s1 d;
    logic [1:0] e;
} s2;

module mod (
    input s1 in,
    input s2 in2,
    input logic clk,
    input s1 in_array[1:0]
);

s1 value1;
s1 value2;
s1 value3;
s1 value4;

assign value1 = in;
assign value2.a = in.a;
assign value4.a = in_array[0].a;
assign value4.b = in2.d.b;

always_ff @(posedge clk) begin
    value3 <= in;
end

always_comb begin
    value2.b = in.b;
end

endmodule
