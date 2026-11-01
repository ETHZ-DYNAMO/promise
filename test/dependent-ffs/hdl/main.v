// `s` always equals `a + b`: all three register the same inputs, and `s`
// registers their sum. Promise should find and prove the invariant
// s == a + b and remove the two FFs of `s`.
module main(input clk, input rst, input x, input y, output [1:0] o,
            output p, output q);
  (* keep *) (* promise *) reg a = 0;
  (* keep *) (* promise *) reg b = 0;
  (* keep *) (* promise *) reg [1:0] s = 0;
  always @(posedge clk)
    if (rst) begin
      a <= 0;
      b <= 0;
      s <= 0;
    end else begin
      a <= x;
      b <= y;
      s <= x + y;
    end
  assign o = s;
  assign p = a;
  assign q = b;
endmodule
