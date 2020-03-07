module mod;

logic [3:0] a;
logic [3:0][1:0] b;
logic [1:0] c;
logic e, f;

// slicing
assign c = a[1:0];
assign a = {1, 1, 1, b[0][0]};

assign b[0][1] = f? a[1]: c[0];

assign b[e] = 1;

endmodule
