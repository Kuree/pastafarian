module PassThrough (
    input logic[15:0] in,
    output logic[15:0] out
);

logic [15:0] value;

always_comb begin
    value = in;
end

assign out = value;

endmodule
