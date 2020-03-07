module mod #(parameter P = 4'h4, P2 = 4'h4)
(
  input logic [P-1:0] in,
  output logic [P2-1:0] out
);

localparam value = 4'd2;

logic [P-1:0] v;
assign v = in;
assign out = v * value;
endmodule   // mod

