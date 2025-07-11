#pragma once

// START: Symbols from libabc.so
#include "base/main/mainInt.h"
#include "promise/StringUtils.h"
#include <filesystem>
#include <iostream>
#include <ostream>
#include <stdexcept>
#if defined(ABC_NAMESPACE)
namespace ABC_NAMESPACE {
#elif defined(__cplusplus)
extern "C" {
#endif

// procedures to start and stop the ABC framework
// (should be called before and after the ABC procedures are called)
void Abc_Start(); // NOLINT
void Abc_Stop();  // NOLINT

// procedures to get the ABC framework and execute commands in it
typedef struct Abc_Frame_t_ Abc_Frame_t; // NOLINT

Abc_Frame_t *Abc_FrameGetGlobalFrame();                          // NOLINT
int Cmd_CommandExecute(Abc_Frame_t *pAbc, const char *sCommand); // NOLINT

#if defined(ABC_NAMESPACE)
}
using namespace ABC_NAMESPACE;
#elif defined(__cplusplus)
}
#endif
// END: Symbols from libabc.so

#include <boost/json.hpp>
#include <iostream>

namespace json = boost::json;

#include "base/abc/abc.h"

inline void runAbcCommand(Abc_Frame_t *pAbc, const std::string &cmd) {
  if (!pAbc) {
    throw std::runtime_error(
        "Error: abc not initialized with Abc_FrameGetGlobalFrame");
  }

  std::cerr << "[ABC] Command: " << cmd << "\n";

  if (int retCode = Cmd_CommandExecute(pAbc, cmd.c_str()))
    throw std::runtime_error("[ABC] Command \"" + cmd + "\" failed with code " +
                             std::to_string(retCode));
  std::cerr << "[ABC] Success!\n";
}

void runAbcPdrProof(const std::string &filename,
                    const std::string &logFileName);

void runrIC3PdrProof(const std::string &blifFileName,
                     const std::string &logFileName);

void runAbcScorrOptimization(const std::string &filename,
                             const std::string &outputFileName,
                             unsigned inductionDepth, bool withInvariants,
                             unsigned numPOs);

void runAbcCombOptimization(const std::string &filename,
                            const std::string &outputFileName);

void runSequentialEquivalenceChecking(const std::string &source,
                                      const std::string &target,
                                      unsigned timeout);

struct SynthResult {
  unsigned nodes;
  unsigned ffs;
  unsigned levels;
  void dumpJson(std::ostream &os) {
    json::object obj;
    obj["nodes"] = nodes;
    obj["ffs"] = ffs;
    obj["levels"] = levels;
    os << json::serialize(obj);
  }
};

struct SynthResultAig : public SynthResult {
  void extractData(Abc_Frame_t *pAbc) {
    // Make a copy of the current network (so "st" won't do anything in
    // place)
    Abc_Ntk_t *pCopy = Abc_NtkDup(Abc_FrameReadNtk(pAbc));
    assert(pAbc && "NULL network");
    runAbcCommand(pAbc, "st");
    Abc_Ntk_t *ntk = Abc_FrameReadNtk(pAbc);
    nodes = Abc_NtkNodeNum(ntk);
    ffs = Abc_NtkLatchNum(ntk);
    levels = Abc_AigLevel(ntk);
    // Recover the copy
    Abc_FrameSetCurrentNetwork(pAbc, pCopy);
  }

  SynthResultAig(Abc_Frame_t *pAbc) { extractData(pAbc); }

  SynthResultAig(const std::filesystem::path &blifPath) {

    assert(blifPath.string().find(".blif") != std::string::npos);

    Abc_Start();
    Abc_Frame_t *pAbc;
    pAbc = Abc_FrameGetGlobalFrame();
    runAbcCommand(pAbc, "read_blif " + blifPath.string());
    extractData(pAbc);
    Abc_Stop();
  }

  void logResult(std::ostream &os) {
    os << "AIG result: ";
    os << " nodes: " << nodes;
    os << " ffs: " << ffs;
    os << " levels: " << levels;
    os << "\n";
  }
};

struct SynthResultLUT6 : public SynthResult {
  void extractData(Abc_Frame_t *pAbc) {
    // Make a copy of the current network (so "if -K 6" won't do anything in
    // place)
    Abc_Ntk_t *pCopy = Abc_NtkDup(Abc_FrameReadNtk(pAbc));
    assert(pAbc && "NULL network");
    runAbcCommand(pAbc, "if -K 6");
    Abc_Ntk_t *ntk = Abc_FrameReadNtk(pAbc);
    nodes = Abc_NtkNodeNum(ntk);
    ffs = Abc_NtkLatchNum(ntk);
    levels = Abc_NtkLevel(ntk);
    // Recover the copy
    Abc_FrameSetCurrentNetwork(pAbc, pCopy);
  }

  SynthResultLUT6(Abc_Frame_t *pAbc) { extractData(pAbc); }

  SynthResultLUT6(const std::filesystem::path &blifPath) {

    assert(blifPath.string().find(".blif") != std::string::npos);

    Abc_Start();
    Abc_Frame_t *pAbc;
    pAbc = Abc_FrameGetGlobalFrame();
    runAbcCommand(pAbc, "read_blif " + blifPath.string());
    extractData(pAbc);
    Abc_Stop();
  }

  void logResult(std::ostream &os) {
    os << "LUT6 result: ";
    os << " nodes: " << nodes;
    os << " ffs: " << ffs;
    os << " levels: " << levels;
    os << "\n";
  }
};
