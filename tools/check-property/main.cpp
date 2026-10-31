#include <boost/program_options.hpp>
#include <iostream>
#include <vector>

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
#include "promise/Invariants.h"
#include "promise/RTLIL/EquipInvariants.h"
#include "promise/ShellUtils.h"
#include "promise/Simulation/VerilatorUtils.h"
#include "promise/StringUtils.h"
#include "promise/Timer.h"

namespace fs = std::filesystem;

namespace po = boost::program_options;

namespace {

struct CmdLineInput {
  std::vector<std::string> inputFiles;
  std::string topName;
  std::string outDir;
};

std::optional<CmdLineInput> parseCmdLine(int argc, char **argv) {
  CmdLineInput s;

  try {
    std::vector<std::string> inputFiles;
    std::string outputFile;

    // 1. Define options
    po::options_description desc("Allowed options");

    // clang-format off
    desc.add_options()
      ("help,h", "Print help message")
      ("top-name,t", po::value<std::string>(&s.topName)->required(), "Name of the top-level module")
      ("output-dir,o", po::value<std::string>(&s.outDir)->required(), "Name of the output directory")
      ("input-files", po::value<std::vector<std::string>>(&s.inputFiles), "Input files");
    // clang-format n

    // 2. Positional arguments (files)
    po::positional_options_description pos;
    pos.add("input-files", -1); // all remaining args

    // 3. Parse
    po::variables_map vm;
    po::store(
        po::command_line_parser(argc, argv).options(desc).positional(pos).run(),
        vm);

    po::notify(vm);

    // 4. Handle help
    if (vm.count("help")) {
      std::cout << "Usage: mytool [options] <files>\n";
      std::cout << desc << "\n";
      return std::nullopt;
    }

    std::cout << "Input files:\n";
    for (const auto &f : s.inputFiles) {
      std::cout << "  " << f << "\n";
      if (!fs::exists(f)) {
        std::cerr << "Error: Verilog file '" << f << "' does not exist.\n";
        return std::nullopt;
      }
    }


  } catch (const po::error &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return std::nullopt;
  }

  return s;
}

} // namespace

int main(int argc, char **argv) {

  auto s = parseCmdLine(argc, argv);

  if (!s) {
    std::cerr << "Error: Failed to parse commandline!\n";
    return 0;
  }

  std::cerr << "Reading Verilog files.\n";
  // Enables Yosys pass outputs
  Yosys::logger().add_sink<Yosys::StreamLogSink>(std::cout);
  // Also print errors (but not warnings) to stderr.
  Yosys::logger().add_sink<Yosys::StderrLogSink>(/*quiet=*/true);

  //
  yosys_setup();
  RTLIL::Design *design = new RTLIL::Design;
  run_pass("read_verilog " + join(s->inputFiles, " "), design);

  RTLIL::Module *m = design->module(RTLIL::escape_id(s->topName));


  run_pass("synth -nofsm -flatten -top " + s->topName +
               "; dffunmap; check -assert",
           design);

  run_pass("clean", design);

  mkdir(s->outDir);

  run_pass("write_verilog -norename " + (fs::path(s->outDir) / "flattened.v").string(), design);


  // Register the set of safety properties
  std::vector<Invariant> safetyProperties;
  for (auto *wire : m->wires()) {
    if (wire->has_attribute(RTLIL::escape_id("promise_safety"))) {
      safetyProperties.emplace_back(mkAst(wire->name));
    }
  }

  for (auto &p : safetyProperties) {
    std::cerr << "Property: " << p.toString() << "\n";
  }

  RTLIL::Module *cloned = m->clone();

  instrumentInvariants(cloned, safetyProperties,
                       /* trimOriginalOutputs = */ true,
                       /* separateInvariants = */ true);

  RTLIL::Design *newDesign = new RTLIL::Design;

  newDesign->add(cloned);

  run_pass("write_verilog -norename " + (fs::path(s->outDir) / "with_properties.v").string(), newDesign);

  auto propertiesBlif = fs::path(s->outDir) / "with_properties.blif";

  run_pass("techmap; write_blif " + propertiesBlif.string(), newDesign);


  yosys_shutdown();

  return 0;
}
