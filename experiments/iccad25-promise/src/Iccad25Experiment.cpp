#include <boost/graph/sequential_vertex_coloring.hpp>

#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <vector>

// Linear Algebra Headers
#include <Eigen/Dense>
#include <Eigen/src/Core/ArithmeticSequence.h>
#include <Eigen/src/Core/Matrix.h>
#include <Eigen/src/Core/MatrixBase.h>

// Yosys Headers
#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"

// Promise headers
#include "promise/AbcCommands.h"
#include "promise/Invariants.h"
#include "promise/RTLIL/EncodingOptimization.h"
#include "promise/RTLIL/EquipInvariants.h"
#include "promise/RTLIL/RTLILUtils.h"
#include "promise/ShellUtils.h"
#include "promise/StringUtils.h"
#include "promise/Timer.h"

#include "Iccad25Experiment.h"

USING_YOSYS_NAMESPACE

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

void runSimulation(const std::filesystem::path &objDir,
                   const std::vector<std::filesystem::path> &verilogSrcs,
                   const std::filesystem::path &testbench,
                   const std::string &topName) {

  // Generate the CPP simulation model
  std::stringstream verilatorCmd;
  verilatorCmd << std::filesystem::path(PROMISE_BINARIES_DIR) / "verilator";
  verilatorCmd << " --trace -Mdir " << objDir;
  verilatorCmd << " --cc ";
  for (const auto &src : verilogSrcs) {
    verilatorCmd << " " << src;
  }
  verilatorCmd << " --exe " << testbench;

  // Some internal signals in xls might begin with an underscore.
  // NOTE: be aware that we also need `--coverage-underscore` at some point
  verilatorCmd << " --trace-underscore";
  verilatorCmd << " --Wno-UNOPTFLAT";
  verilatorCmd << " --top-module " << topName;
  auto [code, stdout] = shell(verilatorCmd.str());

  if (code != 0) {
    std::cerr << "Error: Verilator failed to compile the design.\n" << stdout;
    throw std::runtime_error("Verilator compilation failed");
  }

  // Compile the CPP simulation model
  std::stringstream makeCmd;
  // OBJCACHE=: don't compile through ccache. Every model has the same file
  // names, and ccache returned objects compiled for another model, so the
  // files of one simulator disagreed on the struct layout and it crashed.
  makeCmd << "make -C " << objDir << " OBJCACHE=";
  makeCmd << " -f V" << topName + ".mk";
  makeCmd << " V" << topName;

  auto [makeCode, makeStdout] = shell(makeCmd.str());
  if (makeCode != 0) {
    std::cerr << "Error: failed to build the Verilator model.\n" << makeStdout;
    throw std::runtime_error("Verilator model build failed");
  }

  // Launch the binary:
  std::stringstream simCmd;
  simCmd << (objDir / ("V" + topName));
  {
    Timer t(RUNTIME_SIM);
    runSimulationBinary(simCmd.str());
  }
}

// Attempt to prove the invariant and return the model checking result (which
// contains the analyzed counterexample data if the property failed).
ModelCheckingResult
verifyLinearInvariants(const Iccad25Config &config, RTLIL::Module *m,
                       const std::vector<Invariant> &invariants) {

  RTLIL::Design *design = new RTLIL::Design;

  // Clone the design
  RTLIL::Module *cloned = m->clone();

  design->add(cloned);

  // Equip the module with an invariant and make it an PO. The goal of the model
  // checker is to prove that it is 1 in all reachable states.
  instrumentInvariants(cloned, invariants,
                       /* trimOriginalOutputs = */ true,
                       /* separateInvariants = */ false);

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

  delete design;
  return result;
}

/// \brief A tuple of (FFs, ANDs, max depth).
using AigPrintStatsRes =
    std::tuple</* FFs */ unsigned, /* ANDs */ unsigned, /* Depth */ unsigned>;

/// \brief A tuple of (FFs, LUT6, max depth).
using LUT6PrintStatsRes =
    std::tuple</* FFs */ unsigned, /* ANDs */ unsigned, /* Depth */ unsigned>;

AigPrintStatsRes reportAigUsage(const std::string &blifFile) {

  // Matches the print_stats commands'output
  // Example:
  // main    : i/o =   11/    9  lat =   37  and =   1566  lev = 40
  const std::regex pattern(
      R"(^.*i/o\s*=\s*(\d+)\s*/\s*(\d+)\s*lat\s*=\s*(\d+)\s*and\s*=\s*(\d+)\s*lev\s*=\s*(\d+))");

  std::stringstream cmd;

  cmd << std::filesystem::path(PROMISE_BINARIES_DIR) / "yosys-abc";
  cmd << " -q";
  cmd << " \"read_blif " << blifFile << "; st; print_stats\"";

  auto [retCode, stdout] = shell(cmd.str());

  std::cerr << stdout << "\n";
  if (retCode != 0) {
    throw std::runtime_error("Cannot run ABC command\n");
  }

  stdout = trim(stdout);
  std::smatch match;
  if (std::regex_search(stdout, match, pattern)) {
    auto ffs = std::stoi(match[3]);
    auto ands = std::stoi(match[4]);
    auto depths = std::stoi(match[5]);

    return AigPrintStatsRes(ffs, ands, depths);
  }
  throw std::runtime_error("Cannot parse ABC output\n");
}

std::vector<Invariant> inferInvariants(RTLIL::Module *module,
                                       RTLIL::Design *design,
                                       Iccad25Config &config,
                                       const std::string &topName) {

  // RTLIL::Module *module = design->module(RTLIL::escape_id(topName));

  auto regOuts = getRegOutputs(module);

  // [STEP]: retrieve single-bit register output signals.
  std::vector<RTLIL::IdString> singleBitRegOuts;
  for (auto *sig : regOuts) {
    if (sig->width == 1) {
      singleBitRegOuts.push_back(sig->name);
    }
  }

  // Flattened verilog design: after removing procs and mapping all FFs to
  // simple FFs (used for verilator simulation).
  auto flattenedVerilog = config.getOutputDir() / VERILOG_FLATTENED;
  run_pass("write_verilog " + flattenedVerilog.string(), design);

  // Object directory for SUGGEST
  auto verilatorSimObjDir = config.getCurrentSimDir() / VERILATOR_OBJ_DIR_NAME;

  // [STEP]: Simulate the design:
  std::vector<Eigen::MatrixXi> simData;
  for (size_t i = 0; i < VERILATOR_SIM_ROUNDS; i++, config.newSimIteration()) {
    auto testbenchFile = config.getCurrentSimDir() / VERILATOR_TB_NAME;
    auto vcdFile = config.getCurrentSimDir() / SIM_WAVEFORM;

    // Compile the design into a simulation model:
    createRandomTestBench(testbenchFile, module, VERILATOR_SIM_CYCLES, vcdFile,
                          i);

    runSimulation(verilatorSimObjDir, {flattenedVerilog}, testbenchFile,
                  topName);

    Eigen::MatrixXi signalMatrix =
        vcdToSignalMatrix(module, vcdFile, singleBitRegOuts);
    simData.push_back(signalMatrix);
  }

  auto signalMatrix = getUniqueRows(simData);

  // [STEP]: Suggest invariants from the signalMatrix:
  std::vector<Invariant> linearInvariants;
  {
    Timer t(RUNTIME_LINEAR_EQUALITY);
    linearInvariants =
        inferLinearEqualities(module, signalMatrix, singleBitRegOuts);
  }

  std::vector<Invariant> linearInequalities;
  {
    Timer t(RUNTIME_LINEAR_INEQUALITY);
    linearInequalities = inferLinearInequalitiesViaConflictGraph(
        module, signalMatrix, singleBitRegOuts);
  }

  std::copy(linearInequalities.begin(), linearInequalities.end(),
            std::back_inserter(linearInvariants));

  // [STEP]: Guarantee the correctness of the generated invariants
  ModelCheckingResult modelCheckingResult;

  {
    Timer t(RUNTIME_PROOF);
    modelCheckingResult =
        verifyLinearInvariants(config, module, linearInvariants);
  }
  config.newProofIteration();

  // [STEP]: Iterate between "suggest" and "guarantee" phases to prove the
  // conjunction of invariants:
  while (modelCheckingResult.status == ModelCheckingResult::UNSAFE) {
    auto cexTbFile = config.getCurrentProofDir() / VERILATOR_TB_NAME;
    auto cexVcdFile = config.getCurrentProofDir() / CEX_WAVEFORM;
    auto verilatorObjDir = config.getCurrentProofDir() / VERILATOR_OBJ_DIR_NAME;

    // [STEP]: Collect the states from the latest model checking result
    createCexTestBench(cexTbFile, module, modelCheckingResult, cexVcdFile);
    runSimulation(verilatorObjDir, {flattenedVerilog}, cexTbFile, topName);

    // Retrieve the signal matrix from CEX
    auto cexMatrix = vcdToSignalMatrix(module, cexVcdFile, singleBitRegOuts);
    signalMatrix = getUniqueRows({signalMatrix, cexMatrix});

    // [STEP]: Suggest invariants from the signalMatrix:
    {
      Timer t(RUNTIME_LINEAR_EQUALITY);
      linearInvariants =
          inferLinearEqualities(module, signalMatrix, singleBitRegOuts);
    }

    {
      Timer t(RUNTIME_LINEAR_INEQUALITY);
      linearInequalities = inferLinearInequalitiesViaConflictGraph(
          module, signalMatrix, singleBitRegOuts);
    }
    std::copy(linearInequalities.begin(), linearInequalities.end(),
              std::back_inserter(linearInvariants));

    {
      Timer t(RUNTIME_PROOF);

      modelCheckingResult =
          verifyLinearInvariants(config, module, linearInvariants);
    }

    config.newProofIteration();
  }

  std::cerr << "[" << PROOF_ITERATIONS << "] " << config.getProofIteration()
            << "\n";

  std::cerr << "[INFO] List of proven invariants";
  for (const auto &inv : linearInvariants) {
    std::cerr << "[INFO] invariant: " << inv.toString() << "\n";
  }

  assert(modelCheckingResult.status == ModelCheckingResult::SAFE);
  return linearInvariants;
}

LUT6PrintStatsRes reportLUT6Usage(const std::string &blifFile) {

  const std::regex pattern(
      R"(^.*i/o\s*=\s*(\d+)/\s*(\d+)\s+lat\s*=\s*(\d+)\s+nd\s*=\s*(\d+)\s+edge\s*=\s*(\d+)\s+aig\s*=\s*(\d+)\s+lev\s*=\s*(\d+))");
  std::stringstream cmd;

  cmd << std::filesystem::path(PROMISE_BINARIES_DIR) / "yosys-abc";
  cmd << " -q";
  cmd << " \"read_blif " << blifFile << "; st; if -K 6; print_stats\"";

  auto [retCode, stdout] = shell(cmd.str());

  std::cerr << stdout << "\n";
  if (retCode != 0) {
    throw std::runtime_error("Cannot run ABC command\n");
  }

  stdout = trim(stdout);
  std::smatch match;
  if (std::regex_search(stdout, match, pattern)) {
    auto ffs = std::stoi(match[3]);
    auto luts = std::stoi(match[4]);
    auto depths = std::stoi(match[7]);

    return LUT6PrintStatsRes(ffs, luts, depths);
  }
  throw std::runtime_error("Cannot parse ABC output\n");
}

void extractReportUsageToJson(boost::json::object &obj,
                              const std::filesystem::path &blifFile) {
  //
  auto [st_ffs, st_aigs, st_depth] = reportAigUsage(blifFile);
  auto [fpga_ffs, fpga_luts, fpga_depth] = reportLUT6Usage(blifFile);
  obj[ST_FF] = st_ffs;
  obj[ST_NODE] = st_aigs;
  obj[ST_DEPTH] = st_depth;
  obj[FPGA_FF] = fpga_ffs;
  obj[FPGA_NODE] = fpga_luts;
  obj[FPGA_DEPTH] = fpga_depth;
}

unsigned getNumPOs(RTLIL::Module *module) {
  unsigned numPOBits = 0;
  int maxPortId = -1;
  for (Wire *identifier : module->wires()) {
    if (identifier->port_output) {
      numPOBits += identifier->width;
      maxPortId =
          maxPortId < identifier->port_id ? identifier->port_id : maxPortId;
    }
  }
  return numPOBits;
}

// Technique: Comb
void flowNone(RTLIL::Module *m, const std::filesystem::path &outputFile) {
  RTLIL::Design *design = new RTLIL::Design;
  RTLIL::Module *cloned = m->clone();
  design->add(cloned);
  run_pass("techmap; write_blif /tmp/initial.blif", design);
  runAbcCombOptimization("/tmp/initial.blif", outputFile);

  boost::json::object obj;

  obj[SCORR] = "";
  obj[ENCODING] = "";
  obj[INVARIANT] = "";

  extractReportUsageToJson(obj, outputFile);
  obj[INDUCTION_DEPTH] = 0;
  std::cerr << "[RESULT] " << boost::json::serialize(obj) << std::endl;
}

// Technique: Comb + Scorr
void flowScorr(RTLIL::Module *m, const std::filesystem::path &outputFile,
               unsigned inductionDepth) {
  RTLIL::Design *design = new RTLIL::Design;
  RTLIL::Module *cloned = m->clone();
  design->add(cloned);
  run_pass("techmap; write_blif /tmp/initial.blif", design);

  runAbcScorrOptimization("/tmp/initial.blif", outputFile,
                          /* inductionDepth */ inductionDepth,
                          /* withInvariants */ false, getNumPOs(cloned));

  boost::json::object obj;

  obj[SCORR] = SCORR;
  obj[ENCODING] = "";
  obj[INVARIANT] = "";
  extractReportUsageToJson(obj, outputFile);
  obj[INDUCTION_DEPTH] = inductionDepth;
  std::cerr << "[RESULT] " << boost::json::serialize(obj) << std::endl;
}

// Technique: Comb + Scorr
void flowScorrEncoding(RTLIL::Module *m,
                       const std::filesystem::path &outputFile,
                       const std::vector<Invariant> &invariants,
                       unsigned inductionDepth) {
  RTLIL::Design *design = new RTLIL::Design;

  // Clone the design
  RTLIL::Module *cloned = m->clone();

  design->add(cloned);

  applyEncodingOptimizationUsingInvariants(cloned, invariants);
  run_pass("clean", design);

  run_pass("techmap; write_blif /tmp/encoding_optimized.blif", design);
  runAbcScorrOptimization("/tmp/encoding_optimized.blif", outputFile,
                          /* inductionDepth */ inductionDepth,
                          /* withInvariants */ false, getNumPOs(cloned));

  boost::json::object obj;

  obj[SCORR] = SCORR;
  obj[ENCODING] = ENCODING;
  obj[INVARIANT] = "";
  extractReportUsageToJson(obj, outputFile);
  obj[INDUCTION_DEPTH] = inductionDepth;
  std::cerr << "[RESULT] " << boost::json::serialize(obj) << std::endl;
}

void flowScorrInvar(RTLIL::Module *m, const std::filesystem::path &outputFile,
                    const std::vector<Invariant> &invariants,
                    unsigned inductionDepth) {
  RTLIL::Design *design = new RTLIL::Design;

  // Clone the design
  RTLIL::Module *cloned = m->clone();

  design->add(cloned);

  // Instrument the invariants, without trimming the original outputs
  instrumentInvariants(cloned, invariants,
                       /* trimOriginalOutputs = */ false,
                       /* separateInvariants = */ false);
  auto designWithInvariant =
      outputFile.parent_path() / "design_with_invariant.blif";

  run_pass("techmap; write_blif " + designWithInvariant.string(), design);

  runAbcScorrOptimization(designWithInvariant, outputFile,
                          /* inductionDepth */ inductionDepth,
                          /* withInvariants */ true, getNumPOs(cloned));
  boost::json::object obj;

  obj[SCORR] = SCORR;
  obj[ENCODING] = "";
  obj[INVARIANT] = INVARIANT;
  extractReportUsageToJson(obj, outputFile);
  obj[INDUCTION_DEPTH] = inductionDepth;
  std::cerr << "[RESULT] " << boost::json::serialize(obj) << std::endl;
}

void flowScorrInvarEncoding(RTLIL::Module *m,
                            const std::filesystem::path &outputFile,
                            const std::vector<Invariant> &invariants,
                            unsigned inductionDepth) {
  RTLIL::Design *design = new RTLIL::Design;

  // Clone the design
  RTLIL::Module *cloned = m->clone();

  design->add(cloned);

  // Instrument the invariants, without trimming the original outputs
  instrumentInvariants(cloned, invariants,
                       /* trimOriginalOutputs = */ false,
                       /* separateInvariants = */ false);
  run_pass("clean", design);

  // Apply encoding optimization on top of the circuit with invariants
  applyEncodingOptimizationUsingInvariants(cloned, invariants);
  run_pass("clean", design);

  auto encOptimizedDesignWithInvar =
      outputFile.parent_path() / "design_with_invariant.blif";

  run_pass("techmap; write_blif " + encOptimizedDesignWithInvar.string(),
           design);

  // Apply Scorr and trim the property_pin
  runAbcScorrOptimization(encOptimizedDesignWithInvar, outputFile,
                          /* inductionDepth */ inductionDepth,
                          /* withInvariants */ true, getNumPOs(cloned));

  boost::json::object obj;

  obj[SCORR] = SCORR;
  obj[ENCODING] = ENCODING;
  obj[INVARIANT] = INVARIANT;
  extractReportUsageToJson(obj, outputFile);

  obj[INDUCTION_DEPTH] = inductionDepth;
  std::cerr << "[RESULT] " << boost::json::serialize(obj) << std::endl;
}