module mod(
    input logic clk,
    input logic reset,
    input logic data
);

// a is constant driver
logic a, b, c, d, e, f, g;

always_ff @(posedge clk) begin
    if (reset) begin
        // give them all constants
        a <= 0;
        b <= 0;
        c <= 0;
        d <= 0;
        e <= 0;
        f <= 0;
        g <= 0;
    end
    else begin
        // a is constant driver
        a <= a + 1;
        // b is constant driver
        b <= a;
        // c is not
        c <= c + e;
        // d is not
        d <= d + c;
        // e is not
        e <= e * data;
        // f is constant driver
        f <= a? f: g;
        // g is constant driver
        g <= 1;
    end
end



endmodule
