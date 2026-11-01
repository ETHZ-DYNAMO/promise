/// Unit tests of Promise's invariant-based optimizations.
///
/// Each test builds a small design with Yosys, applies one optimization with
/// a hand-written invariant that holds in the design, and checks that the
/// optimization is effective (it removes FFs) and correct (Yosys proves the
/// optimized design sequentially equivalent to the original by induction).
/// They use no external tools (Verilator, rIC3), so they run in well under a
/// second.

#include "promise/Invariants.h"
#include "promise/RTLIL/EncodingOptimization.h"
#include "promise/RTLIL/ReplaceDependentFFs.h"

#include "kernel/rtlil.h"
#include "kernel/yosys.h"

// After the Yosys headers: ABC defines macros (e.g., `ARGS`) that clash with
// names in Yosys's headers.
#include "promise/AbcCommands.h"
#include "promise/RTLIL/EquipInvariants.h"

#include <gtest/gtest.h>

#include <fstream>
#include <string>

USING_YOSYS_NAMESPACE

namespace {

/// Starts and stops Yosys once for all tests.
class YosysEnvironment : public ::testing::Environment {
public:
  void SetUp() override {
    yosys_setup();
    // Print Yosys errors (e.g., a failed equivalence proof) to stderr.
    logger().add_sink<StderrLogSink>(/*quiet=*/true);
  }
  void TearDown() override { yosys_shutdown(); }
};

/// Synthesizes \p verilog (top module \p top) to `$_DFF_P_` FFs and simple
/// gates, as Promise's flows do, and returns the design.
std::unique_ptr<RTLIL::Design> synthesize(const std::string &verilog,
                                          const std::string &top) {
  // The file is written to the working directory of the test (in build/).
  std::string path = top + ".v";
  std::ofstream(path) << verilog;
  auto design = std::make_unique<RTLIL::Design>();
  run_pass("read_verilog " + path, design.get());
  run_pass("synth -nofsm -flatten -top " + top + "; dffunmap; check -assert",
           design.get());
  return design;
}

/// Returns the number of FF bits in \p module.
int countFFBits(RTLIL::Module *module) {
  int numBits = 0;
  for (auto *cell : module->cells()) {
    if (cell->type == ID($_DFF_P_))
      ++numBits;
    else if (cell->type == ID($dff))
      numBits += GetSize(cell->getPort(ID::Q));
  }
  return numBits;
}

/// Adds a copy of \p module named `gold` to \p design, to compare against
/// after optimizing \p module.
void addGoldCopy(RTLIL::Design *design, RTLIL::Module *module) {
  RTLIL::Module *gold = module->clone();
  gold->name = RTLIL::escape_id("gold");
  design->add(gold);
}

/// Proves that \p gate is sequentially equivalent to the `gold` copy, from
/// the initial values of the FFs (`sat` takes them from the `init`
/// attributes). `sat -verify` aborts the test process if the proof fails.
void proveEquivalentToGold(RTLIL::Design *design, RTLIL::Module *gate) {
  run_pass(std::string("miter -equiv -flatten -make_assert gold ") +
               log_id(gate->name) + " miter",
           design);
  run_pass("sat -verify -tempinduct -prove-asserts miter", design);
}

std::shared_ptr<PropAst> var(const std::string &name) {
  return mkAst(RTLIL::escape_id(name));
}

/// A design where `s` always equals `a + b`: all three register the same
/// inputs, and `s` registers their sum.
const std::string dependentFFsVerilog = R"(
module dep(input clk, input x, input y, output [1:0] o, output p, output q);
  (* keep *) reg a = 0;
  (* keep *) reg b = 0;
  (* keep *) reg [1:0] s = 0;
  // `t` is `s` itself; used to check that cyclic replacements are skipped.
  (* keep *) wire [1:0] t = s;
  always @(posedge clk) begin
    a <= x;
    b <= y;
    s <= x + y;
  end
  assign o = s;
  assign p = a;
  assign q = b;
endmodule
)";

TEST(ReplaceDependentFFs, RemovesFFsOfEqualityInvariant) {
  auto design = synthesize(dependentFFsVerilog, "dep");
  RTLIL::Module *module = design->module(RTLIL::escape_id("dep"));
  ASSERT_EQ(countFFBits(module), 4);
  addGoldCopy(design.get(), module);

  // Invariant: s == a + b
  Invariant inv(mkAst(EqOp(var("s"), mkAst(AddOp(var("a"), var("b"))))));
  EXPECT_EQ(replaceDependentFFs(module, {inv}), 2u);
  run_pass("opt_clean", design.get());
  EXPECT_EQ(countFFBits(module), 2);

  proveEquivalentToGold(design.get(), module);
}

TEST(ReplaceDependentFFs, SkipsReplacementThatCreatesCycle) {
  auto design = synthesize(dependentFFsVerilog, "dep");
  RTLIL::Module *module = design->module(RTLIL::escape_id("dep"));

  // Invariant: s == t, where t is s itself; driving s by t is a cycle.
  Invariant inv(mkAst(EqOp(var("s"), var("t"))));
  EXPECT_EQ(replaceDependentFFs(module, {inv}), 0u);
  EXPECT_EQ(countFFBits(module), 4);
}

/// A one-hot ring of four FFs: exactly one of r0..r3 is 1.
const std::string oneHotRingVerilog = R"(
module ring(input clk, input en, output [3:0] o);
  (* keep *) reg r0 = 1;
  (* keep *) reg r1 = 0;
  (* keep *) reg r2 = 0;
  (* keep *) reg r3 = 0;
  always @(posedge clk)
    if (en) begin
      r0 <= r3;
      r1 <= r0;
      r2 <= r1;
      r3 <= r2;
    end
  assign o = {r3, r2, r1, r0};
endmodule
)";

TEST(EncodingOptimization, EncodesMutuallyExclusiveFFs) {
  auto design = synthesize(oneHotRingVerilog, "ring");
  RTLIL::Module *module = design->module(RTLIL::escape_id("ring"));
  ASSERT_EQ(countFFBits(module), 4);
  addGoldCopy(design.get(), module);

  // Invariant: r0 + r1 + r2 + r3 <= 1
  auto sum = mkAst(AddOp(
      mkAst(AddOp(mkAst(AddOp(var("r0"), var("r1"))), var("r2"))), var("r3")));
  Invariant inv(mkAst(LeOp(sum, mkAst(1))));
  applyEncodingOptimizationUsingInvariants(module, {inv});
  run_pass("clean", design.get());
  // Four mutually exclusive FFs have five states: ceil(log2(5)) = 3 FFs.
  EXPECT_EQ(countFFBits(module), 3);

  proveEquivalentToGold(design.get(), module);
}

/// Two FFs that toggle every cycle from the same initial value: they are
/// sequentially equivalent, but not structurally (different D inputs), so
/// Yosys keeps both.
const std::string togglingFFsVerilog = R"(
module tog(input clk, output o, output p);
  (* keep *) reg a = 0;
  (* keep *) reg a2 = 0;
  always @(posedge clk) begin
    a <= ~a;
    a2 <= ~a2;
  end
  assign o = a;
  assign p = a2;
endmodule
)";

/// Promise's scorr with invariants must merge FFs that are in the cone of an
/// invariant. Plain `scorr -c` drops every merge in the cones of the
/// constraints; Promise uses `-m` (full merge) since its invariants are
/// proven.
TEST(ScorrWithInvariants, MergesEquivalentFFsInInvariantCone) {
  auto design = synthesize(togglingFFsVerilog, "tog");
  RTLIL::Module *module = design->module(RTLIL::escape_id("tog"));
  ASSERT_EQ(countFFBits(module), 2);
  run_pass("techmap; write_blif tog_gold.blif", design.get());

  // Invariant: a == a2. Instrument it as Promise's flows do: as the last PO.
  Invariant inv(mkAst(EqOp(var("a"), var("a2"))));
  instrumentInvariants(module, {inv}, /*trimOriginalOutputs=*/false,
                       /*separateInvariants=*/false);
  unsigned numPOBits = 0;
  for (auto *wire : module->wires())
    if (wire->port_output)
      numPOBits += wire->width;
  run_pass("techmap; write_blif tog_invar.blif", design.get());

  runAbcScorrOptimization("tog_invar.blif", "tog_scorr.blif",
                          /*inductionDepth=*/10, /*withInvariants=*/true,
                          numPOBits);
  EXPECT_EQ(SynthResultLUT6(std::filesystem::path("tog_scorr.blif")).ffs, 1);

  // dsec stores its verdict in the problem status: 1 means equivalent.
  Abc_Start();
  Abc_Frame_t *pAbc = Abc_FrameGetGlobalFrame();
  runAbcCommand(pAbc, "dsec -T 60 tog_gold.blif tog_scorr.blif");
  EXPECT_EQ(Abc_FrameReadProbStatus(pAbc), 1);
  Abc_Stop();
}

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ::testing::AddGlobalTestEnvironment(new YosysEnvironment);
  return RUN_ALL_TESTS();
}
