
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <vector>

#include "base/abc/abc.h"
#include "base/main/main.h"
#include "promise/AbcCommands.h"
#include "promise/ShellUtils.h"
#include "promise/StringUtils.h"
#include "promise/Timer.h"

#include <boost/filesystem.hpp>

void runAbcPdrProof(const std::string &filename,
                    const std::string &logFileName) {
  Abc_Start();

  Abc_Frame_t *pAbc;

  pAbc = Abc_FrameGetGlobalFrame();

  runAbcCommand(pAbc, "read_blif " + filename);
  runAbcCommand(pAbc, "st");

  {
    Timer t("pdr");
    std::string cmd = "pdr -L " + logFileName;
    Cmd_CommandExecute(pAbc, cmd.c_str());
  }

  Abc_Stop();
}

void runrIC3PdrProof(const std::string &blifFileName,
                     const std::string &logFileName) {
  Abc_Start();
  Abc_Frame_t *pAbc;
  pAbc = Abc_FrameGetGlobalFrame();
  runAbcCommand(pAbc, "read_blif " + blifFileName);
  runAbcCommand(pAbc, "st");

  // Create an unique temporary AIG file
  auto aigModelName = boost::filesystem::unique_path("/tmp/model-%%%%%%.aig");

  runAbcCommand(pAbc, "write_aiger " + aigModelName.string());

  Abc_Stop();

  std::stringstream cmd;
  cmd << std::filesystem::path(PROMISE_BINARIES_DIR) / "rIC3"
      << " -e ic3 --witness " << aigModelName;
  auto [retCode, output] = shell(cmd.str());

  if (retCode != 10 && retCode != 20) {
    throw std::runtime_error("Error - rIC3 exited unexpectedly!");
  }

  std::ofstream f(logFileName);
  if (!f.is_open()) {
    throw std::runtime_error("Cannot open file " + logFileName);
  }

  f << output;

  boost::filesystem::remove(aigModelName);
}

void runAbcCombOptimization(const std::string &filename,
                            const std::string &outputFileName) {
  Abc_Start();
  Abc_Frame_t *pAbc;

  pAbc = Abc_FrameGetGlobalFrame();

  runAbcCommand(pAbc, "read_blif " + filename + "");
  runAbcCommand(pAbc, "st");
  runAbcCommand(pAbc, "if -K 6");
  runAbcCommand(pAbc, "ps");
  runAbcCommand(pAbc, "write_blif " + outputFileName);

  Abc_Stop();
}

void runAbcScorrOptimization(const std::string &filename,
                             const std::string &outputFileName,
                             unsigned inductionDepth, bool withInvariants,
                             unsigned numPOs) {
  Abc_Start();

  if (inductionDepth == 0)
    throw std::runtime_error("Induction depth cannot be 0!");

  Abc_Frame_t *pAbc;

  pAbc = Abc_FrameGetGlobalFrame();

  runAbcCommand(pAbc, ("read_blif " + filename));
  runAbcCommand(pAbc, ("st"));

  if (withInvariants) {
    runAbcCommand(pAbc, "constr -N 1");
  }

  std::stringstream cmd;
  cmd << "scorr";
  // -m: also merge nodes in the cones of the constraints. Without it, scorr
  // drops every equivalence there (e.g., FFs that the invariants read). This is
  // sound because Promise's invariants are proven to hold in all reachable
  // states.
  if (withInvariants)
    cmd << " -c -m";
  cmd << " -F " << inductionDepth;
  runAbcCommand(pAbc, cmd.str());

  if (withInvariants) {
    std::stringstream cmd;
    runAbcCommand(pAbc, "constr -r");
    runAbcCommand(pAbc, "zeropo -N " + std::to_string(numPOs - 1));
    runAbcCommand(pAbc, "removepo -N " + std::to_string(numPOs - 1));
  }
  runAbcCommand(pAbc, "if -K 6; ps");
  runAbcCommand(pAbc, "write_blif " + outputFileName);

  Abc_Stop();
}

void runSequentialEquivalenceChecking(const std::string &source,
                                      const std::string &target,
                                      unsigned timeout) {

  Abc_Start();

  Abc_Frame_t *pAbc;

  pAbc = Abc_FrameGetGlobalFrame();

  runAbcCommand(pAbc, "miter " + source + " " + target);

  runAbcCommand(pAbc, "dprove -T " + std::to_string(timeout));

  Abc_Stop();
}
