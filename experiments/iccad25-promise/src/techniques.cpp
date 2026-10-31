
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/wait.h>

#include <boost/graph/sequential_vertex_coloring.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
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
#include "promise/Invariants.h"
#include "promise/Simulation/VcdParser.h"
#include "promise/Simulation/VerilatorUtils.h"
#include "promise/StringUtils.h"
#include "promise/Timer.h"

// Experiment header
#include "Iccad25Experiment.h"

int main(int argc, char *argv[]) {
  Timer t(RUNTIME_TOTAL);
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0]
              << "<top_module_name> <verilog_file1>,<verilog_file2>,...\n";
    return 1;
  }

  std::string topName = argv[1];
  std::string inFileNames = argv[2];

  std::vector<std::string> verilogFiles = split(inFileNames, ',');

  yosys_setup();

  RTLIL::Design *design = new RTLIL::Design;

#define PROMISE_LOG
#ifdef PROMISE_LOG
  // Enables Yosys pass outputs
  Yosys::logger().add_sink<Yosys::StreamLogSink>(std::cout);
  // Also print errors (but not warnings) to stderr.
  Yosys::logger().add_sink<Yosys::StderrLogSink>(/*quiet=*/true);
#endif

  // Run Yosys passes on the design
  run_pass("read_verilog " + join(verilogFiles, " "), design);
  run_pass("synth -nofsm -flatten -top " + topName +
               "; dffunmap; check -assert",
           design);

  RTLIL::Module *module = design->module(RTLIL::escape_id(topName));

  Iccad25Config config(3600, "output");

  auto linearInvariants = inferInvariants(module, design, config, topName);

  // The optimization steps below are not done in-place, i.e., `module`
  // stays the same. We have the following combinations of techniques:
  // - (nothing is applied)
  flowNone(module, config.getSynthResultDir() / "comb.blif");

  // - Scorr
  flowScorr(module, config.getSynthResultDir() / "scorr.blif",
            DEFAULT_INDUCTION_DEPTH);

  // - Scorr + encodingOpt
  flowScorrEncoding(module, config.getSynthResultDir() / "scorr_enc.blif",
                    linearInvariants, DEFAULT_INDUCTION_DEPTH);

  // - Scorr + invariant
  flowScorrInvar(module, config.getSynthResultDir() / "scorr_invar.blif",
                 linearInvariants, DEFAULT_INDUCTION_DEPTH);

  // {
  //   Timer t("sec scorr + invar");
  //   runSequentialEquivalenceChecking(config.getSynthResultDir() /
  //   "comb.blif",
  //                                    config.getSynthResultDir() /
  //                                    "scorr_invar.blif", 3600);
  // }

  // - Scorr + encodingOpt + invariant
  flowScorrInvarEncoding(module,
                         config.getSynthResultDir() / "scorr_invar_enc.blif",
                         linearInvariants, DEFAULT_INDUCTION_DEPTH);

  // applyScorrOptimization(config, module, linearInvariants);

  // applyEncodingOptimization(config, module, linearInvariants);

  delete design;
  yosys_shutdown();

  std::cerr << argv[0] << " exits successfully!\n";
  return 0;
}
