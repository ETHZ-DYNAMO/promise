#include "kernel/rtlil.h"

#include "promise/AbcCommands.h"
#include <base/abc/abc.h>
#include <vector>

USING_YOSYS_NAMESPACE

struct AigToRTLILObjs {

  unsigned simulationCycles;

  /// Original module
  RTLIL::Module *module;

  Abc_Frame_t *pAbcObj;

  size_t resetPinId;

  /// Maps PI in aig to SigBit in the Yosys module
  std::vector<RTLIL::SigBit> inputs;
  std::vector<bool> useXValue;

  /// Maps Reg in aig to SigBit in the Yosys module
  std::vector<RTLIL::SigBit> regs;

  /// Maps Reg in aig to SigBit in Yosys module
  /// std::vector<RTLIL::SigBit> outputs;

  /// - module: Original module
  /// - controlPrefixes: A list of name prefixes for signals that are not
  /// simulated with x
  AigToRTLILObjs(RTLIL::Module *module, Abc_Frame_t *pAbcObj,
                 unsigned simulationCycles,
                 const std::vector<std::string> &controlPrefixes);
};

struct AigSim {

  ///
  AigToRTLILObjs mapping;

  AigSim();
};

void runXSim(const std::string &filename, RTLIL::Module *module,
             const std::vector<std::string> &controlPrefixes);
