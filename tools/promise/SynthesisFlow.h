#include <filesystem>
#include <utility>

// Yosys Headers
#include "kernel/log.h"
#include "kernel/register.h"
#include "kernel/rtlil.h"
#include "kernel/yosys.h"

#include "promise/ModelCheckingResult.h"
#include "promise/ShellUtils.h"

const std::string VERILOG_FLATTENED = "flattened.v";
const std::string VERILATOR_TB_NAME = "main.cpp";
const std::string VERILATOR_OBJ_DIR_NAME = "obj_dir";
const std::string SIM_BINARY_NAME = "sim.out";
const std::string SIM_WAVEFORM = "sim.vcd";
const std::string CEX_WAVEFORM = "cex.vcd";

class SynthesisFlowConfig {
  // Model checking timeout in seconds
  unsigned modelCheckingTimeOut = 3600;

  // The number of iterations that Promise simulates the input design
  unsigned simIteration = 0;

  // The number of iterations that Promise attempted to prove the invariant(s)
  unsigned proofIteration = 0;

  // Location for all output files
  std::filesystem::path outputDir;

public:
  // Get the path to the directory to write temporary simulation data:
  std::filesystem::path getCurrentSimDir() const {
    auto dirName = outputDir / ("sim_round_" + std::to_string(simIteration));
    mkdir(dirName);
    return dirName;
  }

  // Get the path to the directory to write temporary verification data:
  std::filesystem::path getCurrentProofDir() const {
    auto dirName =
        outputDir / ("verif_round_" + std::to_string(proofIteration));
    mkdir(dirName);
    return dirName;
  }

  // Get the path to the directory to write temporary verification data:
  std::filesystem::path getCurrentDebugDir() const {
    auto dirName =
        outputDir / ("verif_round_" + std::to_string(proofIteration)) / "debug";
    mkdir(dirName);
    return dirName;
  }

  std::filesystem::path getSynthResultDir() const {
    auto dirName = outputDir / ("synthesis_result");
    mkdir(dirName);
    return dirName;
  }

  // Get the path to the output directory
  std::filesystem::path getOutputDir() const {
    mkdir(outputDir);
    return outputDir;
  }

  SynthesisFlowConfig(unsigned modelCheckingTimeOut,
                      std::filesystem::path outputDir)
      : modelCheckingTimeOut(modelCheckingTimeOut),
        outputDir(std::move(outputDir)) {}

  void newProofIteration() { proofIteration += 1; }
  void newSimIteration() { simIteration += 1; }
};

bool synthFlowSingleOutputReg(SynthesisFlowConfig config, RTLIL::Design *design,
                              const std::string &topName);

bool synthFlowManual(SynthesisFlowConfig config, RTLIL::Design *design,
                     const std::string &topName);