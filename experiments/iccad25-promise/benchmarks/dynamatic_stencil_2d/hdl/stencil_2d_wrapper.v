module stencil_2d_wrapper(
  input [7:0] orig_din0,
  input [7:0] orig_din1,
  input [7:0] filter_din0,
  input [7:0] filter_din1,
  input [7:0] sol_din0,
  input [7:0] sol_din1,
  input  orig_start_valid,
  input  filter_start_valid,
  input  sol_start_valid,
  input  start_valid,
  input  clk,
  input  rst,
  input  out0_ready,
  input  orig_end_ready,
  input  filter_end_ready,
  input  sol_end_ready,
  input  end_ready,
  output  orig_start_ready,
  output  filter_start_ready,
  output  sol_start_ready,
  output  start_ready,
  output [7:0] out0,
  output  out0_valid,
  output  orig_end_valid,
  output  filter_end_valid,
  output  sol_end_valid,
  output  end_valid,
  output  orig_ce0,
  output  orig_we0,
  output [9:0] orig_address0,
  output [7:0] orig_dout0,
  output  orig_ce1,
  output  orig_we1,
  output [9:0] orig_address1,
  output [7:0] orig_dout1,
  output  filter_ce0,
  output  filter_we0,
  output [3:0] filter_address0,
  output [7:0] filter_dout0,
  output  filter_ce1,
  output  filter_we1,
  output [3:0] filter_address1,
  output [7:0] filter_dout1,
  output  sol_ce0,
  output  sol_we0,
  output [9:0] sol_address0,
  output [7:0] sol_dout0,
  output  sol_ce1,
  output  sol_we1,
  output [9:0] sol_address1,
  output [7:0] sol_dout1
);
  wire  mem_to_bram_converter_filter_ce0;
  wire  mem_to_bram_converter_filter_we0;
  wire [3:0] mem_to_bram_converter_filter_address0;
  wire [7:0] mem_to_bram_converter_filter_dout0;
  wire  mem_to_bram_converter_filter_ce1;
  wire  mem_to_bram_converter_filter_we1;
  wire [3:0] mem_to_bram_converter_filter_address1;
  wire [7:0] mem_to_bram_converter_filter_dout1;
  wire [7:0] mem_to_bram_converter_filter_loadData;
  wire  mem_to_bram_converter_sol_ce0;
  wire  mem_to_bram_converter_sol_we0;
  wire [9:0] mem_to_bram_converter_sol_address0;
  wire [7:0] mem_to_bram_converter_sol_dout0;
  wire  mem_to_bram_converter_sol_ce1;
  wire  mem_to_bram_converter_sol_we1;
  wire [9:0] mem_to_bram_converter_sol_address1;
  wire [7:0] mem_to_bram_converter_sol_dout1;
  wire [7:0] mem_to_bram_converter_sol_loadData;
  wire  mem_to_bram_converter_orig_ce0;
  wire  mem_to_bram_converter_orig_we0;
  wire [9:0] mem_to_bram_converter_orig_address0;
  wire [7:0] mem_to_bram_converter_orig_dout0;
  wire  mem_to_bram_converter_orig_ce1;
  wire  mem_to_bram_converter_orig_we1;
  wire [9:0] mem_to_bram_converter_orig_address1;
  wire [7:0] mem_to_bram_converter_orig_dout1;
  wire [7:0] mem_to_bram_converter_orig_loadData;
  wire [7:0] stencil_2d_wrapped_out0;
  wire  stencil_2d_wrapped_out0_valid;
  wire  stencil_2d_wrapped_out0_ready;
  wire  stencil_2d_wrapped_orig_end_valid;
  wire  stencil_2d_wrapped_orig_end_ready;
  wire  stencil_2d_wrapped_filter_end_valid;
  wire  stencil_2d_wrapped_filter_end_ready;
  wire  stencil_2d_wrapped_sol_end_valid;
  wire  stencil_2d_wrapped_sol_end_ready;
  wire  stencil_2d_wrapped_end_valid;
  wire  stencil_2d_wrapped_end_ready;
  wire  stencil_2d_wrapped_orig_loadEn;
  wire [9:0] stencil_2d_wrapped_orig_loadAddr;
  wire  stencil_2d_wrapped_orig_storeEn;
  wire [9:0] stencil_2d_wrapped_orig_storeAddr;
  wire [7:0] stencil_2d_wrapped_orig_storeData;
  wire  stencil_2d_wrapped_filter_loadEn;
  wire [3:0] stencil_2d_wrapped_filter_loadAddr;
  wire  stencil_2d_wrapped_filter_storeEn;
  wire [3:0] stencil_2d_wrapped_filter_storeAddr;
  wire [7:0] stencil_2d_wrapped_filter_storeData;
  wire  stencil_2d_wrapped_sol_loadEn;
  wire [9:0] stencil_2d_wrapped_sol_loadAddr;
  wire  stencil_2d_wrapped_sol_storeEn;
  wire [9:0] stencil_2d_wrapped_sol_storeAddr;
  wire [7:0] stencil_2d_wrapped_sol_storeData;

  assign out0 = stencil_2d_wrapped_out0;
  assign out0_valid = stencil_2d_wrapped_out0_valid;
  assign stencil_2d_wrapped_out0_ready = out0_ready;
  assign orig_end_valid = stencil_2d_wrapped_orig_end_valid;
  assign stencil_2d_wrapped_orig_end_ready = orig_end_ready;
  assign filter_end_valid = stencil_2d_wrapped_filter_end_valid;
  assign stencil_2d_wrapped_filter_end_ready = filter_end_ready;
  assign sol_end_valid = stencil_2d_wrapped_sol_end_valid;
  assign stencil_2d_wrapped_sol_end_ready = sol_end_ready;
  assign end_valid = stencil_2d_wrapped_end_valid;
  assign stencil_2d_wrapped_end_ready = end_ready;
  assign orig_ce0 = mem_to_bram_converter_orig_ce0;
  assign orig_we0 = mem_to_bram_converter_orig_we0;
  assign orig_address0 = mem_to_bram_converter_orig_address0;
  assign orig_dout0 = mem_to_bram_converter_orig_dout0;
  assign orig_ce1 = mem_to_bram_converter_orig_ce1;
  assign orig_we1 = mem_to_bram_converter_orig_we1;
  assign orig_address1 = mem_to_bram_converter_orig_address1;
  assign orig_dout1 = mem_to_bram_converter_orig_dout1;
  assign filter_ce0 = mem_to_bram_converter_filter_ce0;
  assign filter_we0 = mem_to_bram_converter_filter_we0;
  assign filter_address0 = mem_to_bram_converter_filter_address0;
  assign filter_dout0 = mem_to_bram_converter_filter_dout0;
  assign filter_ce1 = mem_to_bram_converter_filter_ce1;
  assign filter_we1 = mem_to_bram_converter_filter_we1;
  assign filter_address1 = mem_to_bram_converter_filter_address1;
  assign filter_dout1 = mem_to_bram_converter_filter_dout1;
  assign sol_ce0 = mem_to_bram_converter_sol_ce0;
  assign sol_we0 = mem_to_bram_converter_sol_we0;
  assign sol_address0 = mem_to_bram_converter_sol_address0;
  assign sol_dout0 = mem_to_bram_converter_sol_dout0;
  assign sol_ce1 = mem_to_bram_converter_sol_ce1;
  assign sol_we1 = mem_to_bram_converter_sol_we1;
  assign sol_address1 = mem_to_bram_converter_sol_address1;
  assign sol_dout1 = mem_to_bram_converter_sol_dout1;

  mem_to_bram #(.DATA_WIDTH(8), .ADDR_WIDTH(4)) mem_to_bram_converter_filter(
    .loadEn (stencil_2d_wrapped_filter_loadEn),
    .loadAddr (stencil_2d_wrapped_filter_loadAddr),
    .storeEn (stencil_2d_wrapped_filter_storeEn),
    .storeAddr (stencil_2d_wrapped_filter_storeAddr),
    .storeData (stencil_2d_wrapped_filter_storeData),
    .din0 (filter_din0),
    .din1 (filter_din1),
    .ce0 (mem_to_bram_converter_filter_ce0),
    .we0 (mem_to_bram_converter_filter_we0),
    .address0 (mem_to_bram_converter_filter_address0),
    .dout0 (mem_to_bram_converter_filter_dout0),
    .ce1 (mem_to_bram_converter_filter_ce1),
    .we1 (mem_to_bram_converter_filter_we1),
    .address1 (mem_to_bram_converter_filter_address1),
    .dout1 (mem_to_bram_converter_filter_dout1),
    .loadData (mem_to_bram_converter_filter_loadData)
  );

  mem_to_bram #(.DATA_WIDTH(8), .ADDR_WIDTH(10)) mem_to_bram_converter_sol(
    .loadEn (stencil_2d_wrapped_sol_loadEn),
    .loadAddr (stencil_2d_wrapped_sol_loadAddr),
    .storeEn (stencil_2d_wrapped_sol_storeEn),
    .storeAddr (stencil_2d_wrapped_sol_storeAddr),
    .storeData (stencil_2d_wrapped_sol_storeData),
    .din0 (sol_din0),
    .din1 (sol_din1),
    .ce0 (mem_to_bram_converter_sol_ce0),
    .we0 (mem_to_bram_converter_sol_we0),
    .address0 (mem_to_bram_converter_sol_address0),
    .dout0 (mem_to_bram_converter_sol_dout0),
    .ce1 (mem_to_bram_converter_sol_ce1),
    .we1 (mem_to_bram_converter_sol_we1),
    .address1 (mem_to_bram_converter_sol_address1),
    .dout1 (mem_to_bram_converter_sol_dout1),
    .loadData (mem_to_bram_converter_sol_loadData)
  );

  mem_to_bram #(.DATA_WIDTH(8), .ADDR_WIDTH(10)) mem_to_bram_converter_orig(
    .loadEn (stencil_2d_wrapped_orig_loadEn),
    .loadAddr (stencil_2d_wrapped_orig_loadAddr),
    .storeEn (stencil_2d_wrapped_orig_storeEn),
    .storeAddr (stencil_2d_wrapped_orig_storeAddr),
    .storeData (stencil_2d_wrapped_orig_storeData),
    .din0 (orig_din0),
    .din1 (orig_din1),
    .ce0 (mem_to_bram_converter_orig_ce0),
    .we0 (mem_to_bram_converter_orig_we0),
    .address0 (mem_to_bram_converter_orig_address0),
    .dout0 (mem_to_bram_converter_orig_dout0),
    .ce1 (mem_to_bram_converter_orig_ce1),
    .we1 (mem_to_bram_converter_orig_we1),
    .address1 (mem_to_bram_converter_orig_address1),
    .dout1 (mem_to_bram_converter_orig_dout1),
    .loadData (mem_to_bram_converter_orig_loadData)
  );

  stencil_2d stencil_2d_wrapped(
    .orig_loadData (mem_to_bram_converter_orig_loadData),
    .filter_loadData (mem_to_bram_converter_filter_loadData),
    .sol_loadData (mem_to_bram_converter_sol_loadData),
    .orig_start_valid (orig_start_valid),
    .orig_start_ready (orig_start_ready),
    .filter_start_valid (filter_start_valid),
    .filter_start_ready (filter_start_ready),
    .sol_start_valid (sol_start_valid),
    .sol_start_ready (sol_start_ready),
    .start_valid (start_valid),
    .start_ready (start_ready),
    .clk (clk),
    .rst (rst),
    .out0 (stencil_2d_wrapped_out0),
    .out0_valid (stencil_2d_wrapped_out0_valid),
    .out0_ready (stencil_2d_wrapped_out0_ready),
    .orig_end_valid (stencil_2d_wrapped_orig_end_valid),
    .orig_end_ready (stencil_2d_wrapped_orig_end_ready),
    .filter_end_valid (stencil_2d_wrapped_filter_end_valid),
    .filter_end_ready (stencil_2d_wrapped_filter_end_ready),
    .sol_end_valid (stencil_2d_wrapped_sol_end_valid),
    .sol_end_ready (stencil_2d_wrapped_sol_end_ready),
    .end_valid (stencil_2d_wrapped_end_valid),
    .end_ready (stencil_2d_wrapped_end_ready),
    .orig_loadEn (stencil_2d_wrapped_orig_loadEn),
    .orig_loadAddr (stencil_2d_wrapped_orig_loadAddr),
    .orig_storeEn (stencil_2d_wrapped_orig_storeEn),
    .orig_storeAddr (stencil_2d_wrapped_orig_storeAddr),
    .orig_storeData (stencil_2d_wrapped_orig_storeData),
    .filter_loadEn (stencil_2d_wrapped_filter_loadEn),
    .filter_loadAddr (stencil_2d_wrapped_filter_loadAddr),
    .filter_storeEn (stencil_2d_wrapped_filter_storeEn),
    .filter_storeAddr (stencil_2d_wrapped_filter_storeAddr),
    .filter_storeData (stencil_2d_wrapped_filter_storeData),
    .sol_loadEn (stencil_2d_wrapped_sol_loadEn),
    .sol_loadAddr (stencil_2d_wrapped_sol_loadAddr),
    .sol_storeEn (stencil_2d_wrapped_sol_storeEn),
    .sol_storeAddr (stencil_2d_wrapped_sol_storeAddr),
    .sol_storeData (stencil_2d_wrapped_sol_storeData)
  );

endmodule
