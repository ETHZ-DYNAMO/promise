`define BSV_POSITIVE_RESET

module main(
clk,
rst,

EN_getIReq,
getIReq,
RDY_getIReq,

getIResp_a,
EN_getIResp,
RDY_getIResp,

EN_getDReq,
getDReq,
RDY_getDReq,

getDResp_a,
EN_getDResp,
RDY_getDResp,

d2e_writes_to_reg,
e2w_writes_to_reg
);

input clk;
input rst;

input EN_getIReq;
output [67 : 0] getIReq;
output RDY_getIReq;

input [67 : 0] getIResp_a;
input EN_getIResp;
output RDY_getIResp;

input EN_getDReq;
output [67 : 0] getDReq;
output RDY_getDReq;

input [67 : 0] getDResp_a;
input EN_getDResp;
output RDY_getDResp;

output [31 : 0] d2e_writes_to_reg;
output [31 : 0] e2w_writes_to_reg;


rv32_bsv inst(
  .clk(clk),
  .rst(rst),
  .EN_getIReq(EN_getIReq),
  .getIReq(getIReq),
  .RDY_getIReq(RDY_getIReq),
  .getIResp_a(getIResp_a),
  .EN_getIResp(EN_getIResp),
  .RDY_getIResp(RDY_getIResp),
  .EN_getDReq(EN_getDReq),
  .getDReq(getDReq),
  .RDY_getDReq(RDY_getDReq),
  .getDResp_a(getDResp_a),
  .EN_getDResp(EN_getDResp),
  .RDY_getDResp(RDY_getDResp),
  .d2e_writes_to_reg(d2e_writes_to_reg),
  .e2w_writes_to_reg(e2w_writes_to_reg)
);

endmodule
