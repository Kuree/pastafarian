module child (
    input logic a,
    output logic b
);

assign b = a;

endmodule


module mod;

logic[3:0] a;
logic[3:0] b;

genvar i;
generate 
for (i = 0; i < 4; i += 1) begin :block
    child c(.a(a[i]), .b(b[i]));
end :block
endgenerate

endmodule
