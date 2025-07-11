#include <boost/graph/sequential_vertex_coloring.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
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
#include "kernel/register.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"
#include "kernel/yosys_common.h"

// Promise headers
#include "SynthesisFlow.h"
#include "promise/Simulation/VerilatorUtils.h"
#include "promise/StringUtils.h"
#include "promise/Timer.h"

namespace fs = std::filesystem;

USING_YOSYS_NAMESPACE

int main(int argc, char *argv[]) {
  Timer t("total");
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0]
              << " <top_module_name> <verilog_file1> <verilog_file2> ...\n";
    return 1;
  }

  std::string topName = argv[1];
  std::string inFileNames = argv[2];

  std::vector<std::string> verilogFiles(argv + 2, argv + argc);

  // runVerilatorLinting(verilogFiles, topName);

  for (auto &f : verilogFiles) {
    if (!fs::exists(f)) {
      std::cerr << "Error: Verilog file '" << f << "' does not exist.\n";
      return 1;
    }
  }

  yosys_setup();

  RTLIL::Design *design = new RTLIL::Design;

#define PROMISE_LOG
#ifdef PROMISE_LOG
  // Enables Yosys pass outputs
  Yosys::log_streams.push_back(&std::cout);
  Yosys::log_error_stderr = true;
#endif

  // Run Yosys passes on the design
  run_pass("read_verilog " + join(verilogFiles, " "), design);

  SynthesisFlowConfig config(3600, "output");

  synthFlowManual(config, design, topName);

  delete design;
  yosys_shutdown();

  std::cerr << argv[0] << " exits successfully!\n";
  return 0;
}