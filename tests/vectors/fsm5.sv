module mod (
    input logic clk,
    input logic rst_n,
    input logic wen,
    input logic ren
);

typedef enum logic[1:0] {
  IDLE = 2'h0,
  MODIFY = 2'h1,
  READ = 2'h2
} r_w_seq_state;

r_w_seq_state r_w_seq_current_state;
r_w_seq_state r_w_seq_next_state;

always_ff @(posedge clk, negedge rst_n) begin
  if (!rst_n) begin
    r_w_seq_current_state <= IDLE;
  end
  else r_w_seq_current_state <= r_w_seq_next_state;
end
always_comb begin
  r_w_seq_next_state = r_w_seq_current_state;
  unique case (r_w_seq_current_state)
    IDLE: if ((~wen) & (~ren)) begin
      r_w_seq_next_state = IDLE;
    end
    else if (wen) begin
      r_w_seq_next_state = MODIFY;
    end
    else if (ren & (~wen)) begin
      r_w_seq_next_state = READ;
    end
    MODIFY: if (1'h1) begin
      r_w_seq_next_state = IDLE;
    end
    READ: if ((~wen) & (~ren)) begin
      r_w_seq_next_state = IDLE;
    end
    else if (wen) begin
      r_w_seq_next_state = MODIFY;
    end
    else if (ren & (~wen)) begin
      r_w_seq_next_state = READ;
    end
    default: r_w_seq_next_state = r_w_seq_current_state;
  endcase
end


endmodule
