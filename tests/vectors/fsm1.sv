module mod (
  input logic clk,
  input logic [1:0] in,
  output logic [1:0] out,
  input logic rst
);

typedef enum logic {
  Blue = 1'h0,
  Red = 1'h1
} Color_state;
Color_state Color_current_state;
Color_state Color_next_state;

always_ff @(posedge clk, posedge rst) begin
  if (rst) begin
    Color_current_state <= Red;
  end
  else Color_current_state <= Color_next_state;
end
always_comb begin
  unique case (Color_current_state)
    Blue: if (in == 2'h1) begin
      Color_next_state = Red;
    end
    else Color_next_state = Blue;
    Red: if (in == 2'h1) begin
      Color_next_state = Blue;
    end
    else if (in == 2'h0) begin
      Color_next_state = Red;
    end
    else Color_next_state = Red;
  endcase
end
always_comb begin
  unique case (Color_current_state)
    Blue: begin :Color_Blue_Output
        out = 2'h1;
      end :Color_Blue_Output
    Red: begin :Color_Red_Output
        out = 2'h2;
      end :Color_Red_Output
  endcase
end
endmodule   // mod

