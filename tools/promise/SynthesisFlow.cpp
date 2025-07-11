
#include "kernel/log.h"
#include "promise/AbcCommands.h"
#include "promise/Invariants.h"
#include "promise/ModelCheckingResult.h"
#include "promise/ShellUtils.h"

#include "promise/RTLIL/EncodingOptimization.h"
#include "promise/RTLIL/EquipInvariants.h"
#include "promise/RTLIL/RTLILUtils.h"

#include "promise/Simulation/VcdParser.h"
#include "promise/Simulation/VerilatorUtils.h"

#include "promise/Timer.h"

#include "kernel/rtlil.h"
#include "kernel/yosys.h"

#include "SynthesisFlow.h"
#include <Eigen/src/Core/Matrix.h>
#include <iostream>
#include <kernel/ff.h>
#include <kernel/ffinit.h>
#include <kernel/sigtools.h>
#include <tuple>
#include <utility>

/// \brief: A helper class for selecting signals to be analyzed to produce
/// invariants
struct SignalList {
  std::vector<IdString> signals;
  RTLIL::Module *module;
  SignalList(Module *m) : module(m) { SigMap map(m); }

  /// \brief: Add the name newSig to the list of the signals if there is no
  /// duplicated signals.
  ///
  /// \note: assert false if the IdString does not correspond to a wire in the
  /// module
  void addSignal(const IdString &newSig) {
    std::string sigName = log_id(newSig);
    if (sigName[0] == '$') {
      return;
    }

    if (sigName.find("clk") != std::string::npos ||
        sigName.find("rst") != std::string::npos ||
        sigName.find("CLK") != std::string::npos ||
        sigName.find("RST") != std::string::npos) {
      return;
    }

    assert(module->wire(newSig) &&
           "Attempting to add a signal by name that is not a wire!");
    // The signal is unique
    if (std::all_of(
            signals.begin(), signals.end(), [newSig, this](const IdString &id) {
              return SigSpec(module->wire(newSig)) != SigSpec(module->wire(id));
            })) {
      signals.push_back(newSig);
    }
  }

  void addBscRdyEnSignals() {
    for (auto *wire : module->wires()) {
      std::string strName = log_id(wire->name);
      if ((strName.find("EN_") != std::string::npos ||
           strName.find("RDY_") != std::string::npos ||
           strName.find("ENC") != std::string::npos ||
           strName.find("DEC") != std::string::npos) &&
          !wire->port_input) {
        assert(wire->width == 1);
        addSignal(wire->name);
      }
    }
  }

  void addSingleBitRegOuts() {
    for (auto *wire : getRegOutputs(module)) {
      if (wire->width == 1) {
        auto *ffCell = getDriverCell(module, wire);
        assert(checkFFInitialized(module, ffCell));
        addSignal(wire->name);
      }
    }
  }

  void addSingleBitSignals() {
    for (auto *wire : module->wires()) {
      if (wire->width == 1) {
        addSignal(wire->name);
      }
    }
  }

  /// \brief: This adds all the signals marked with (* promise *) to the list of
  /// signals to derive invariants.
  void addSignalsMarkedWithPromise() {
    for (auto *wire : module->wires()) {
      if (wire->has_attribute(RTLIL::escape_id("promise"))) {
        addSignal(wire->name);
      }
    }
  }

private:
  SigMap map;
};

// Compressing the unique rows from a vector of matrices into one matrix
Eigen::MatrixXi getUniqueRows(const vector<Eigen::MatrixXi> &matrices) {

  struct VectorCompare {
    bool operator()(const vector<int> &a, const vector<int> &b) const {
      return a < b;
    }
  };

  std::set<vector<int>, VectorCompare> uniqueRows;
  for (const auto &mat : matrices) {
    for (int i = 0; i < mat.rows(); ++i) {
      vector<int> row(mat.cols());
      for (int j = 0; j < mat.cols(); ++j) {
        row[j] = mat(i, j);
      }
      uniqueRows.insert(row);
    }
  }

  int numRows = uniqueRows.size();
  int numCols = uniqueRows.empty() ? 0 : uniqueRows.begin()->size();

  Eigen::MatrixXi result(numRows, numCols);
  int i = 0;
  for (const auto &row : uniqueRows) {
    for (int j = 0; j < numCols; ++j) {
      result(i, j) = row[j];
    }
    ++i;
  }
  return result;
}

// Attempt to prove the invariant and return the model checking result (which
// contains the analyzed counterexample data if the property failed).
ModelCheckingResult verifyInvariant(const SynthesisFlowConfig &config,
                                    RTLIL::Module *m,
                                    const std::vector<Invariant> &invariants) {
  if (invariants.empty()) {
    std::cerr
        << "Warning - verifying a set of empty invariants! Returning SAFE\n";
    return ModelCheckingResult::trivialInvariant();
  }

  RTLIL::Design *design = new RTLIL::Design;

  // Clone the design
  RTLIL::Module *cloned = m->clone();

  design->add(cloned);

  // Equip the module with an invariant and make it an PO. The goal of the model
  // checker is to prove that it is 1 in all reachable states.
  instrumentInvariants(cloned, invariants,
                       /* trimOriginalOutputs = */ true,
                       /* separateInvariants = */ false);

  design->rename(cloned, RTLIL::escape_id("to_verify"));

  auto miterVerilog = config.getCurrentProofDir() / "to_verify.v";
  run_pass("opt_clean; write_verilog " + miterVerilog.string(), design);

  auto miterBlif = config.getCurrentProofDir() / "to_verify.blif";
  run_pass("techmap; write_blif " + miterBlif.string(), design);

  auto pdrLogFile = config.getCurrentProofDir() / "pdr.log";

// #define USING_ABC_PDR
#ifdef USING_ABC_PDR
  runAbcPdrProof(miterBlif, pdrLogFile);
  ModelCheckingResult result =
      ModelCheckingResult::parseAbcLogFile(m, pdrLogFile);
#else
  runrIC3PdrProof(miterBlif, pdrLogFile);
  ModelCheckingResult result =
      ModelCheckingResult::parserIC3LogFile(m, pdrLogFile);
#endif

#if 0
  // This is used to debug the testbench for counterexample
  if (result.status == ModelCheckingResult::UNSAFE) {
    auto cexTbFile = config.getCurrentDebugDir() / VERILATOR_TB_NAME;
    auto vcdFile = config.getCurrentDebugDir() / SIM_WAVEFORM;
    auto verilatorObjDir = config.getCurrentDebugDir() / VERILATOR_OBJ_DIR_NAME;
    auto testbenchFile = config.getCurrentDebugDir() / VERILATOR_TB_NAME;

    createCexTestBench(cexTbFile, cloned, result, vcdFile);
    buildVerilatorModel(verilatorObjDir, {miterVerilog}, testbenchFile,
                        "to_verify");
    std::stringstream simCmd;
    simCmd << (verilatorObjDir / ("Vto_verify"));
    shell(simCmd.str());
  }
#endif // if 0

  delete design;
  return result;
}

void flowComb(const SynthesisFlowConfig &config, const RTLIL::Module *m) {
  RTLIL::Design *design = new RTLIL::Design;

  // Clone the design
  RTLIL::Module *cloned = m->clone();

  design->add(cloned);

  auto initialBlif = config.getSynthResultDir() / "initial.blif";
  run_pass("techmap; write_blif " + initialBlif.string(), design);

  auto combinationalBaseline =
      config.getSynthResultDir() / "combinational.blif";

  runAbcCombOptimization(initialBlif, combinationalBaseline);
}

// Technique: Comb
void flowScorr(const SynthesisFlowConfig &config, RTLIL::Module *m,
               unsigned inductionDepth) {
  RTLIL::Design *design = new RTLIL::Design;
  RTLIL::Module *cloned = m->clone();
  design->add(cloned);
  run_pass("techmap; write_blif /tmp/initial.blif", design);

  unsigned numPOBits = 0;
  int maxPortId = -1;
  for (Wire *identifier : cloned->wires()) {
    if (identifier->port_output) {
      numPOBits += identifier->width;
      maxPortId =
          maxPortId < identifier->port_id ? identifier->port_id : maxPortId;
    }
  }

  runAbcScorrOptimization("/tmp/initial.blif",
                          config.getSynthResultDir() / "scorr.blif",
                          /* inductionDepth */ inductionDepth,
                          /* withInvariants */ false, numPOBits);
  std::cerr << "[RESULT] after Scorr: ";
  SynthResultLUT6(config.getSynthResultDir() / "scorr.blif")
      .logResult(std::cerr);
}

// Assuming that the conjunction of the invariants is proven, use scorr in ABC
// with invariants to optimize the design
void flowScorrInvar(const SynthesisFlowConfig &config, RTLIL::Module *m,
                    const std::vector<Invariant> &invariants) {

  RTLIL::Design *design = new RTLIL::Design;

  // Clone the design
  RTLIL::Module *cloned = m->clone();

  design->add(cloned);

  // Instrument the invariants, without trimming the original outputs
  instrumentInvariants(cloned, invariants,
                       /* trimOriginalOutputs = */ false,
                       /* separateInvariants = */ false);

  auto miterVerilog = config.getSynthResultDir() / "design_invariants.v";
  run_pass("write_verilog " + miterVerilog.string(), design);

  auto miterBlif = config.getSynthResultDir() / "design_invariants.blif";
  run_pass("techmap; write_blif " + miterBlif.string(), design);

  auto optimizedBlif =
      config.getSynthResultDir() / "design_invariants_scorred.blif";

  unsigned numPOBits = 0;
  int maxPortId = -1;
  for (Wire *identifier : cloned->wires()) {
    if (identifier->port_output) {
      numPOBits += identifier->width;
      maxPortId =
          maxPortId < identifier->port_id ? identifier->port_id : maxPortId;
    }
  }
  // for (auto cell : )
  auto *propertyPin = cloned->wire(RTLIL::escape_id("property_pin"));
  assert(propertyPin->port_id == maxPortId);

  runAbcScorrOptimization(miterBlif, optimizedBlif, /* inductionDepth */ 10,
                          /* withInvariants */ true, numPOBits);

  auto result = SynthResultLUT6(optimizedBlif);
  std::cerr << "[RESULT] after Scorr + Invar: ";
  result.logResult(std::cerr);
}

void flowEncoding(const SynthesisFlowConfig &config, RTLIL::Module *m,
                  const std::vector<Invariant> &invariants) {
  RTLIL::Design *design = new RTLIL::Design;

  // Clone the design
  RTLIL::Module *cloned = m->clone();

  design->add(cloned);

  auto originalBlif = config.getSynthResultDir() / "original.blif";
  run_pass("techmap; write_blif " + originalBlif.string(), design);

  run_pass("clean", design);

  auto outputDebug = config.getSynthResultDir() / "design_debug.v";
  run_pass("write_verilog " + outputDebug.string(), design);

  applyEncodingOptimizationUsingInvariants(cloned, invariants);
  run_pass("clean", design);

  auto outputVerilog = config.getSynthResultDir() / "design_encoding.v";
  run_pass("write_verilog " + outputVerilog.string(), design);

  auto outputBlif = config.getSynthResultDir() / "design_encoding.blif";
  run_pass("techmap; write_blif " + outputBlif.string(), design);

  runSequentialEquivalenceChecking(originalBlif, outputBlif, 3600);

  std::cerr << "[RESULT] after Scorr + Encoding: ";
  SynthResultLUT6(outputBlif).logResult(std::cerr);
}

Eigen::MatrixXi runRandomSimulation(
    RTLIL::Module *m, const std::string &topName, SynthesisFlowConfig &config,
    const std::filesystem::path &flattenedVerilog,
    const std::vector<RTLIL::IdString> &signalList, const unsigned simCycles) {
  // [STEP]: Simulate the design:
  std::vector<Eigen::MatrixXi> simData;
  for (unsigned i = 0; i < 4; i++, config.newSimIteration()) {
    // Object directory for SUGGEST
    auto verilatorSimObjDir =
        config.getCurrentSimDir() / VERILATOR_OBJ_DIR_NAME;
    auto testbenchFile = config.getCurrentSimDir() / VERILATOR_TB_NAME;
    auto vcdFile = config.getCurrentSimDir() / SIM_WAVEFORM;

    // Compile the design into a simulation model:
    createRandomTestBench(testbenchFile, m, simCycles, vcdFile, i);

    buildVerilatorModel(verilatorSimObjDir, {flattenedVerilog}, testbenchFile,
                        topName);

    // Launch the binary:
    std::stringstream simCmd;
    simCmd << (verilatorSimObjDir / ("V" + topName));
    shell(simCmd.str());

    Eigen::MatrixXi signalMatrix = vcdToSignalMatrix(m, vcdFile, signalList);
    simData.push_back(signalMatrix);
  }

  return getUniqueRows(simData);
}

std::vector<Invariant> inferLinearInvariantsFromSimulation(
    RTLIL::Module *m, SynthesisFlowConfig &config, Eigen::MatrixXi signalMatrix,
    const std::vector<RTLIL::IdString> &signalList,
    const std::filesystem::path &flattenedVerilog, const std::string &topName) {
  // [STEP]: Suggest invariants from the signalMatrix:
  std::vector<Invariant> linearInvariants =
      inferLinearEqualities(m, signalMatrix, signalList);

  std::vector<Invariant> linearInequalities =
      inferLinearInequalitiesViaConflictGraph(m, signalMatrix, signalList);

  std::copy(linearInequalities.begin(), linearInequalities.end(),
            std::back_inserter(linearInvariants));

  // [STEP]: Guarantee the correctness of the generated invariants
  ModelCheckingResult modelCheckingResult =
      verifyInvariant(config, m, linearInvariants);
  config.newProofIteration();

  // [STEP]: Iterate between "suggest" and "guarantee" phases to prove the
  // conjunction of invariants:
  while (modelCheckingResult.status == ModelCheckingResult::UNSAFE) {
    auto cexTbFile = config.getCurrentProofDir() / VERILATOR_TB_NAME;
    auto vcdFile = config.getCurrentProofDir() / SIM_WAVEFORM;
    auto verilatorObjDir = config.getCurrentProofDir() / VERILATOR_OBJ_DIR_NAME;
    auto testbenchFile = config.getCurrentProofDir() / VERILATOR_TB_NAME;

    // [STEP]: Collect the states from the latest model checking result
    createCexTestBench(cexTbFile, m, modelCheckingResult, vcdFile);
    buildVerilatorModel(verilatorObjDir, {flattenedVerilog}, testbenchFile,
                        topName);

    // Launch the binary:
    std::stringstream simCmd;
    simCmd << (verilatorObjDir / ("V" + topName));
    shell(simCmd.str());

    // Retrieve the signal matrix from CEX
    auto cexMatrix = vcdToSignalMatrix(m, vcdFile, signalList);
    assert(cexMatrix.rows() > 0 && "Empty CEX!");
    unsigned rowsBefore = signalMatrix.rows();
    std::cerr << "Rows before :" << signalMatrix.rows() << "\n";
    signalMatrix = getUniqueRows({signalMatrix, cexMatrix});
    std::cerr << "Rows after :" << signalMatrix.rows() << "\n";
    assert(signalMatrix.rows() > rowsBefore);

    // [STEP]: Suggest invariants from the signalMatrix:
    linearInvariants = inferLinearEqualities(m, signalMatrix, signalList);
    linearInequalities =
        inferLinearInequalitiesViaConflictGraph(m, signalMatrix, signalList);
    std::copy(linearInequalities.begin(), linearInequalities.end(),
              std::back_inserter(linearInvariants));

    std::cerr << "[INFO] <==== Attempting to verify invariants: ====> \n";
    for (const auto &inv : linearInvariants) {
      std::cerr << "[INFO] invariant: " << inv.toString() << "\n";
    }

    modelCheckingResult = verifyInvariant(config, m, linearInvariants);

    config.newProofIteration();
  }
  assert(modelCheckingResult.status == ModelCheckingResult::SAFE);

  return linearInvariants;
}

// This is the "suggest" and "guarnatee" step
bool synthFlowSingleOutputReg(SynthesisFlowConfig config, RTLIL::Design *design,
                              const std::string &topName) {

  RTLIL::Module *m = design->module(RTLIL::escape_id(topName));

  run_pass("synth -nofsm -flatten -top " + topName +
               "; dffunmap; check -assert",
           design);

  run_pass("clean", design);

  auto flattenedVerilog = config.getOutputDir() / VERILOG_FLATTENED;
  run_pass("write_verilog -norename " + flattenedVerilog.string(), design);

  // Check if all the FFs are initialized

  flowComb(config, m);

  SignalList sigList(m);
  sigList.addSingleBitRegOuts();

  auto signalMatrix = runRandomSimulation(m, topName, config, flattenedVerilog,
                                          sigList.signals, 3);

  auto linearInvariants = inferLinearInvariantsFromSimulation(
      m, config, signalMatrix, sigList.signals, flattenedVerilog, topName);

  std::cerr << "[INFO] List of proven invariants:\n";
  for (const auto &inv : linearInvariants) {
    std::cerr << "[INFO] invariant: " << inv.toString() << "\n";
  }

  flowScorrInvar(config, m, linearInvariants);

  flowEncoding(config, m, linearInvariants);
  return true;
}

// This is the "suggest" and "guarnatee" step
bool synthFlowManual(SynthesisFlowConfig config, RTLIL::Design *design,
                     const std::string &topName) {

  RTLIL::Module *m = design->module(RTLIL::escape_id(topName));

  run_pass("synth -nofsm -flatten -top " + topName +
               "; dffunmap; check -assert",
           design);

  run_pass("clean", design);
  run_pass("opt_clean", design);

  flowComb(config, m);

  SignalList sigList(m);

  // sigList.addSignalsMarkedWithPromise();
  sigList.addSingleBitRegOuts();
  sigList.addBscRdyEnSignals();
  // sigList.addSingleBitSignals();

  for (const auto &s : sigList.signals) {
    std::cerr << "signal to watch: " << log_id(s) << "\n";
  }

  auto flattenedVerilog = config.getOutputDir() / VERILOG_FLATTENED;
  run_pass("write_verilog -norename " + flattenedVerilog.string(), design);

  auto signalMatrix = runRandomSimulation(m, topName, config, flattenedVerilog,
                                          sigList.signals, 25000);

  auto linearInvariants = inferLinearInvariantsFromSimulation(
      m, config, signalMatrix, sigList.signals, flattenedVerilog, topName);

  std::cerr << "[INFO] <==== List of proven invariants: ====> \n";
  for (const auto &inv : linearInvariants) {
    std::cerr << "[INFO] invariant: " << inv.toString() << "\n";
  }

  flowScorr(config, m, 10);
  flowScorrInvar(config, m, linearInvariants);
  return true;
}
