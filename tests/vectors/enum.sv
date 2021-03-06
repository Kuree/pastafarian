typedef enum logic[1:0] {
  IDLE = 2'h0,
  WAIT = 2'h1,
  WORK = 2'h2
} State;

module mod (
  input State in,
  output State out
);

typedef enum logic[1:0] {
  red = 2'h1,
  green = 2'h2
} color;

color c;
assign c = red;

assign out = in;
endmodule   // mod

