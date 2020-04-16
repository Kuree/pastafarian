// hand-adapted from RocketChip DCache
module dcache (
input wire reset,
input wire clock,
input wire releaseDone,
input wire[1:0] s1_meta_hit_state_state,
input wire s2_victim_dirty,
input wire discard_line,
input wire[1:0] probe_bits_param,
input wire[1:0] s2_probe_state_state,
input wire resetting,
input wire s2_valid_hit_pre_data_ecc_and_waw,
input wire s2_update_meta,
input wire auto_out_d_bits_denied,
input wire grantIsCached,
input wire d_done,
input wire s2_want_victimize,
input wire auto_out_b_valid,
input wire block_probe_for_core_progress,
input wire block_probe_for_ordering,
input wire s1_valid,
input wire s2_valid
);

reg[2:0] release_state;
reg s1_probe;
reg s2_probe;

wire s2_prb_ack_data;
wire metaArb_io_in_0_valid;
wire metaArb_io_in_2_valid;
wire metaArb_io_in_3_valid;
wire metaArb_io_in_4_valid;
wire metaArb_io_in_4_ready;
wire metaArb_io_in_6_ready;
wire metaArb__T_1;
wire metaArb__T_2;
wire metaArb__T_3;
wire tl_out__b_ready;

wire _T_3027;
wire _T_3018;
wire _T_2982;
wire _T_2983;
wire _T_2984;
wire _T_2981;
wire _T_2980;
wire _T_2979;
wire _T_2975;
wire _T_2971;
wire[2:0] _T_2972;
wire[2:0] _T_2974;
wire _T_2966;
wire[2:0] _T_2967;
wire _T_2918;
wire _T_2919;
wire _T_2920;
wire _T_2826;
wire _T_768;
wire _T_765;
wire _T_760;
wire _T_761;
wire _T_764;
wire _T_757;
wire _T_756;
wire _T_753;
wire _T_749;
wire _T_748;
wire _T_745;
wire _T_752;
wire _T_736;
wire _T_741;
wire _T_740;
wire _T_744;
wire [3:0] _T_711;
wire _T_49;

wire[2:0] _GEN_278;
wire[2:0] _GEN_257;
wire[2:0] _GEN_255;
wire[2:0] _GEN_288;
wire[2:0] _GEN_238;
wire _GEN_289;
wire[2:0] _GEN_293;
wire[2:0] _GEN_297;
wire[2:0] _GEN_295;

assign _T_3027 = metaArb_io_in_4_ready & metaArb_io_in_4_valid; // @[Decoupled.scala 40:37:freechips.rocketchip.system.DefaultConfig.fir@166141.4]
assign _T_2982 = release_state == 3'h1; // @[package.scala 15:47:freechips.rocketchip.system.DefaultConfig.fir@166057.4]
assign _T_2983 = release_state == 3'h6; // @[package.scala 15:47:freechips.rocketchip.system.DefaultConfig.fir@166058.4]
assign _T_2984 = _T_2982 | _T_2983; // @[package.scala 64:59:freechips.rocketchip.system.DefaultConfig.fir@166059.4]
assign _T_2981 = release_state == 3'h2; // @[DCache.scala 739:25:freechips.rocketchip.system.DefaultConfig.fir@166050.4]
assign _T_2980 = release_state == 3'h3; // @[DCache.scala 734:25:freechips.rocketchip.system.DefaultConfig.fir@166042.4]
assign _T_2979 = release_state == 3'h5; // @[DCache.scala 730:25:freechips.rocketchip.system.DefaultConfig.fir@166035.4]
assign _T_2975 = release_state == 3'h4; // @[DCache.scala 721:25:freechips.rocketchip.system.DefaultConfig.fir@166022.4]
assign _T_2971 = s2_probe_state_state > 2'h0; // @[Metadata.scala 50:45:freechips.rocketchip.system.DefaultConfig.fir@166004.10]
assign _T_2966 = s2_victim_dirty & ~discard_line; // @[DCache.scala 701:44:freechips.rocketchip.system.DefaultConfig.fir@165980.6]
assign _T_2982 = release_state == 3'h1; // @[package.scala 15:47:freechips.rocketchip.system.DefaultConfig.fir@166057.4]
assign _T_2983 = release_state == 3'h6; // @[package.scala 15:47:freechips.rocketchip.system.DefaultConfig.fir@166058.4]
assign _T_2984 = _T_2982 | _T_2983; // @[package.scala 64:59:freechips.rocketchip.system.DefaultConfig.fir@166059.4]
assign _T_2972 = releaseDone ? 3'h7 : 3'h3; // @[DCache.scala 713:29:freechips.rocketchip.system.DefaultConfig.fir@166008.12]
assign _T_2974 = releaseDone ? 3'h0 : 3'h5; // @[DCache.scala 717:29:freechips.rocketchip.system.DefaultConfig.fir@166015.12]
assign _T_2967 = _T_2966 ? 3'h1 : 3'h6; // @[DCache.scala 701:27:freechips.rocketchip.system.DefaultConfig.fir@165981.6]
assign _T_3018 = release_state == 3'h7; // @[package.scala 15:47:freechips.rocketchip.system.DefaultConfig.fir@166123.4]
assign _T_2826 = grantIsCached & d_done; // @[DCache.scala 644:43:freechips.rocketchip.system.DefaultConfig.fir@165754.4]

assign s2_prb_ack_data = _T_768 | _T_765; // @[Misc.scala 36:9:freechips.rocketchip.system.DefaultConfig.fir@163302.4]
assign _GEN_255 = _T_2971 ? _T_2972 : _T_2974; // @[DCache.scala 710:45:freechips.rocketchip.system.DefaultConfig.fir@166005.10]
assign _GEN_278 = s2_probe ? _GEN_257 : _GEN_238; // @[DCache.scala 704:21:freechips.rocketchip.system.DefaultConfig.fir@165992.4]
assign _GEN_257 = s2_prb_ack_data ? 3'h2 : _GEN_255; // @[DCache.scala 708:36:freechips.rocketchip.system.DefaultConfig.fir@166000.8]
assign _GEN_288 = metaArb_io_in_6_ready ? 3'h0 : _GEN_278; // @[DCache.scala 725:37:freechips.rocketchip.system.DefaultConfig.fir@166030.6]
assign _GEN_289 = metaArb_io_in_6_ready | _T_49; // @[DCache.scala 725:37:freechips.rocketchip.system.DefaultConfig.fir@166030.6]
assign _GEN_293 = _T_2975 ? _GEN_288 : _GEN_278; // @[DCache.scala 721:44:freechips.rocketchip.system.DefaultConfig.fir@166023.4]
assign _GEN_297 = _T_2979 ? _GEN_295 : _GEN_293; // @[DCache.scala 730:47:freechips.rocketchip.system.DefaultConfig.fir@166036.4]
assign _GEN_295 = releaseDone ? 3'h0 : _GEN_293; // @[DCache.scala 732:26:freechips.rocketchip.system.DefaultConfig.fir@166038.6]
assign _GEN_238 = s2_want_victimize ? _T_2967 : release_state; // @[DCache.scala 698:25:freechips.rocketchip.system.DefaultConfig.fir@165964.4]

assign s2_prb_ack_data = _T_768 | _T_765; // @[Misc.scala 36:9:freechips.rocketchip.system.DefaultConfig.fir@163302.4]
assign _T_768 = 4'h3 == _T_711; // @[Misc.scala 54:20:freechips.rocketchip.system.DefaultConfig.fir@163301.4]
assign _T_764 = 4'h2 == _T_711; // @[Misc.scala 54:20:freechips.rocketchip.system.DefaultConfig.fir@163297.4]
assign _T_765 = _T_764 ? 1'h0 : _T_761; // @[Misc.scala 36:9:freechips.rocketchip.system.DefaultConfig.fir@163298.4]
assign _T_761 = _T_760 ? 1'h0 : _T_757; // @[Misc.scala 36:9:freechips.rocketchip.system.DefaultConfig.fir@163294.4]
assign _T_711 = {probe_bits_param,s2_probe_state_state}; // @[Cat.scala 29:58:freechips.rocketchip.system.DefaultConfig.fir@163244.4]
assign _T_757 = _T_756 ? 1'h0 : _T_753; // @[Misc.scala 36:9:freechips.rocketchip.system.DefaultConfig.fir@163290.4]
assign _T_756 = 4'h0 == _T_711; // @[Misc.scala 54:20:freechips.rocketchip.system.DefaultConfig.fir@163289.4]
assign _T_749 = _T_748 ? 1'h0 : _T_745; // @[Misc.scala 36:9:freechips.rocketchip.system.DefaultConfig.fir@163282.4]
assign _T_752 = 4'h7 == _T_711; // @[Misc.scala 54:20:freechips.rocketchip.system.DefaultConfig.fir@163285.4]
assign _T_748 = 4'h6 == _T_711; // @[Misc.scala 54:20:freechips.rocketchip.system.DefaultConfig.fir@163281.4]
assign _T_753 = _T_752 | _T_749; // @[Misc.scala 36:9:freechips.rocketchip.system.DefaultConfig.fir@163286.4]
assign _T_49 = tl_out__b_ready & auto_out_b_valid; // @[Decoupled.scala 40:37:freechips.rocketchip.system.DefaultConfig.fir@162193.4]
assign _T_741 = _T_740 ? 1'h0 : _T_736; // @[Misc.scala 36:9:freechips.rocketchip.system.DefaultConfig.fir@163274.4]
assign _T_745 = _T_744 ? 1'h0 : _T_741; // @[Misc.scala 36:9:freechips.rocketchip.system.DefaultConfig.fir@163278.4]
assign _T_736 = 4'hb == _T_711; // @[Misc.scala 54:20:freechips.rocketchip.system.DefaultConfig.fir@163269.4]
assign _T_741 = _T_740 ? 1'h0 : _T_736; // @[Misc.scala 36:9:freechips.rocketchip.system.DefaultConfig.fir@163274.4]
assign _T_740 = 4'h4 == _T_711; // @[Misc.scala 54:20:freechips.rocketchip.system.DefaultConfig.fir@163273.4]
assign _T_744 = 4'h5 == _T_711; // @[Misc.scala 54:20:freechips.rocketchip.system.DefaultConfig.fir@163277.4]


assign metaArb_io_in_0_valid = resetting; // @[DCache.scala 905:26:freechips.rocketchip.system.DefaultConfig.fir@166333.4]
assign metaArb_io_in_2_valid = s2_valid_hit_pre_data_ecc_and_waw & s2_update_meta; // @[DCache.scala 390:26:freechips.rocketchip.system.DefaultConfig.fir@163449.4]
assign metaArb_io_in_3_valid = _T_2826 & ~auto_out_d_bits_denied; // @[DCache.scala 644:26:freechips.rocketchip.system.DefaultConfig.fir@165757.4]
assign metaArb_io_in_4_valid = _T_2983 | _T_3018; // @[DCache.scala 770:26:freechips.rocketchip.system.DefaultConfig.fir@166125.4]
assign metaArb__T_1 = metaArb_io_in_0_valid | metaArb_io_in_2_valid; // @[Arbiter.scala 31:68:freechips.rocketchip.system.DefaultConfig.fir@161568.4]
assign metaArb__T_2 = metaArb__T_1 | metaArb_io_in_3_valid; // @[Arbiter.scala 31:68:freechips.rocketchip.system.DefaultConfig.fir@161569.4]
assign metaArb__T_3 = metaArb__T_2 | metaArb_io_in_4_valid; // @[Arbiter.scala 31:68:freechips.rocketchip.system.DefaultConfig.fir@161570.4]
assign metaArb_io_in_4_ready = ~metaArb__T_2; // @[Arbiter.scala 134:14:freechips.rocketchip.system.DefaultConfig.fir@161589.4]
assign metaArb_io_in_6_ready = ~metaArb__T_3; // @[Arbiter.scala 134:14:freechips.rocketchip.system.DefaultConfig.fir@161593.4]
assign tl_out__b_ready = metaArb_io_in_6_ready & ~_T_2920; // @[DCache.scala 673:44:freechips.rocketchip.system.DefaultConfig.fir@165876.4]
assign _T_2918 = block_probe_for_core_progress | block_probe_for_ordering; // @[DCache.scala 673:79:freechips.rocketchip.system.DefaultConfig.fir@165872.4]
assign _T_2919 = _T_2918 | s1_valid; // @[DCache.scala 673:107:freechips.rocketchip.system.DefaultConfig.fir@165873.4]
assign _T_2920 = _T_2919 | s2_valid; // @[DCache.scala 673:119:freechips.rocketchip.system.DefaultConfig.fir@165874.4]

always @(posedge clock) begin
if (reset) begin
  s1_probe <= 1'h0;
end else if (_T_2975) begin
  s1_probe <= _GEN_289;
end else begin
  s1_probe <= _T_49;
end
if (reset) begin
  s2_probe <= 1'h0;
end else begin
  s2_probe <= s1_probe;
end
if (reset) begin
  release_state <= 3'h0;
end else if (_T_3027) begin
  release_state <= 3'h0;
end else if (_T_2984) begin
  if (releaseDone) begin
    release_state <= 3'h6;
  end else if (_T_2981) begin
    if (releaseDone) begin
      release_state <= 3'h7;
    end else if (_T_2980) begin
      if (releaseDone) begin
        release_state <= 3'h7;
      end else if (_T_2979) begin
        if (releaseDone) begin
          release_state <= 3'h0;
        end else if (_T_2975) begin
          if (metaArb_io_in_6_ready) begin
            release_state <= 3'h0;
          end else if (s2_probe) begin
            if (s2_prb_ack_data) begin
              release_state <= 3'h2;
            end else if (_T_2971) begin
              if (releaseDone) begin
                release_state <= 3'h7;
              end else begin
                release_state <= 3'h3;
              end
            end else if (releaseDone) begin
              release_state <= 3'h0;
            end else begin
              release_state <= 3'h5;
            end
          end else if (s2_want_victimize) begin
            if (_T_2966) begin
              release_state <= 3'h1;
            end else begin
              release_state <= 3'h6;
            end
          end
        end else if (s2_probe) begin
          if (s2_prb_ack_data) begin
            release_state <= 3'h2;
          end else if (_T_2971) begin
            if (releaseDone) begin
              release_state <= 3'h7;
            end else begin
              release_state <= 3'h3;
            end
          end else if (releaseDone) begin
            release_state <= 3'h0;
          end else begin
            release_state <= 3'h5;
          end
        end else if (s2_want_victimize) begin
          if (_T_2966) begin
            release_state <= 3'h1;
          end else begin
            release_state <= 3'h6;
          end
        end
      end else if (_T_2975) begin
        if (metaArb_io_in_6_ready) begin
          release_state <= 3'h0;
        end else if (s2_probe) begin
          if (s2_prb_ack_data) begin
            release_state <= 3'h2;
          end else if (_T_2971) begin
            if (releaseDone) begin
              release_state <= 3'h7;
            end else begin
              release_state <= 3'h3;
            end
          end else if (releaseDone) begin
            release_state <= 3'h0;
          end else begin
            release_state <= 3'h5;
          end
        end else if (s2_want_victimize) begin
          if (_T_2966) begin
            release_state <= 3'h1;
          end else begin
            release_state <= 3'h6;
          end
        end
      end else if (s2_probe) begin
        if (s2_prb_ack_data) begin
          release_state <= 3'h2;
        end else if (_T_2971) begin
          if (releaseDone) begin
            release_state <= 3'h7;
          end else begin
            release_state <= 3'h3;
          end
        end else if (releaseDone) begin
          release_state <= 3'h0;
        end else begin
          release_state <= 3'h5;
        end
      end else if (s2_want_victimize) begin
        if (_T_2966) begin
          release_state <= 3'h1;
        end else begin
          release_state <= 3'h6;
        end
      end
    end else if (_T_2979) begin
      if (releaseDone) begin
        release_state <= 3'h0;
      end else if (_T_2975) begin
        if (metaArb_io_in_6_ready) begin
          release_state <= 3'h0;
        end else begin
          release_state <= _GEN_278;
        end
      end else begin
        release_state <= _GEN_278;
      end
    end else if (_T_2975) begin
      if (metaArb_io_in_6_ready) begin
        release_state <= 3'h0;
      end else begin
        release_state <= _GEN_278;
      end
    end else begin
      release_state <= _GEN_278;
    end
  end else if (_T_2980) begin
    if (releaseDone) begin
      release_state <= 3'h7;
    end else if (_T_2979) begin
      if (releaseDone) begin
        release_state <= 3'h0;
      end else begin
        release_state <= _GEN_293;
      end
    end else begin
      release_state <= _GEN_293;
    end
  end else if (_T_2979) begin
    if (releaseDone) begin
      release_state <= 3'h0;
    end else begin
      release_state <= _GEN_293;
    end
  end else begin
    release_state <= _GEN_293;
  end
end else if (_T_2981) begin
  if (releaseDone) begin
    release_state <= 3'h7;
  end else if (_T_2980) begin
    if (releaseDone) begin
      release_state <= 3'h7;
    end else begin
      release_state <= _GEN_297;
    end
  end else begin
    release_state <= _GEN_297;
  end
end else if (_T_2980) begin
  if (releaseDone) begin
    release_state <= 3'h7;
  end else begin
    release_state <= _GEN_297;
  end
end else begin
  release_state <= _GEN_297;
end
end
endmodule
