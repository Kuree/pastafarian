module TOP;
wire clock, reset, io_in;
wire[1:0] io_out;

DetectTwoOnes dut(clock, reset, io_in, io_out);

endmodule



module DetectTwoOnes( // @[:@3.2]
  input   clock, // @[:@4.4]
  input   reset, // @[:@5.4]
  input   io_in, // @[:@6.4]
  output[1:0]  io_out // @[:@6.4]
);
  reg [1:0] state; // @[DetectTwoOnes.scala 11:22:@8.4]
  reg [31:0] _RAND_0;
  wire  _T_11; // @[Conditional.scala 37:30:@11.4]
  wire [1:0] _GEN_0; // @[DetectTwoOnes.scala 17:20:@13.6]
  wire  _T_12; // @[Conditional.scala 37:30:@18.6]
  wire [1:0] _GEN_1; // @[DetectTwoOnes.scala 22:20:@20.8]
  wire  _T_13; // @[Conditional.scala 37:30:@28.8]
  wire  _T_15; // @[DetectTwoOnes.scala 29:13:@30.10]
  wire [1:0] _GEN_2; // @[DetectTwoOnes.scala 29:21:@31.10]
  wire [1:0] _GEN_3; // @[Conditional.scala 39:67:@29.8]
  wire [1:0] _GEN_4; // @[Conditional.scala 39:67:@19.6]
  wire [1:0] _GEN_5; // @[Conditional.scala 40:58:@12.4]
  assign _T_11 = 2'h0 == state; // @[Conditional.scala 37:30:@11.4]
  assign _GEN_0 = io_in ? 2'h1 : state; // @[DetectTwoOnes.scala 17:20:@13.6]
  assign _T_12 = 2'h1 == state; // @[Conditional.scala 37:30:@18.6]
  assign _GEN_1 = io_in ? 2'h2 : 2'h0; // @[DetectTwoOnes.scala 22:20:@20.8]
  assign _T_13 = 2'h2 == state; // @[Conditional.scala 37:30:@28.8]
  assign _T_15 = io_in == 1'h0; // @[DetectTwoOnes.scala 29:13:@30.10]
  assign _GEN_2 = _T_15 ? 2'h0 : state; // @[DetectTwoOnes.scala 29:21:@31.10]
  assign _GEN_3 = _T_13 ? _GEN_2 : state; // @[Conditional.scala 39:67:@29.8]
  assign _GEN_4 = _T_12 ? _GEN_1 : _GEN_3; // @[Conditional.scala 39:67:@19.6]
  assign _GEN_5 = _T_11 ? _GEN_0 : _GEN_4; // @[Conditional.scala 40:58:@12.4]
  assign io_out = {state == 2'h2, 1'd0}; // @[DetectTwoOnes.scala 13:10:@10.4]
  always @(posedge clock) begin
    if (reset) begin
      state <= 2'h0;
    end else begin
      if (_T_11) begin
        if (io_in) begin
          state <= 2'h1;
        end
      end else begin
        if (_T_12) begin
          if (io_in) begin
            state <= 2'h2;
          end else begin
            state <= 2'h0;
          end
        end else begin
          if (_T_13) begin
            if (_T_15) begin
              state <= 2'h0;
            end
          end
        end
      end
    end
  end
endmodule
