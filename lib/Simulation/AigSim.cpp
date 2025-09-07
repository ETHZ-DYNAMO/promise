#include "promise/Simulation/AigSim.h"
#include "promise/AbcCommands.h"
#include "promise/RTLIL/RTLILUtils.h"

static void xSimulation(AigToRTLILObjs helper);
static inline int abcXsimRand2();
static inline int abcXsimAnd(int value0, int value1);
static inline void abcObjSetXsim(Abc_Obj_t *pObj, int value);
static inline int abcObjGetXsim(Abc_Obj_t *pObj);
static inline int abcXsimInv(int value);
static inline int abcObjGetXsimFanin0(Abc_Obj_t *pObj);
static inline int abcObjGetXsimFanin1(Abc_Obj_t *pObj);
static inline void abcXsimPrint(FILE *pFile, int value);

AigToRTLILObjs::AigToRTLILObjs(RTLIL::Module *module, Abc_Frame_t *pAbcObj,
                               unsigned simulationCycles,
                               const std::vector<std::string> &controlPrefixes)
    : simulationCycles(simulationCycles), module(module), pAbcObj(pAbcObj) {

  int pi = 0;
  for (Wire *input : getSortedInput(module)) {
    for (int pos = 0; pos < input->width; ++pos) {
      bool useX = true;
      std::string sigName = log_id(input);
      if (sigName.find("rst") != std::string::npos ||
          sigName.find("reset") != std::string::npos) {
        std::cerr << "Input wire " << sigName << " with id = " << pos
                  << " is a reset pin!\n";
        resetPinId = pi;
      }
      for (const std::string &prefix : controlPrefixes) {
        // If the signal has a control prefix, we simulate using random binary
        // values instead of X.
        if (sigName.find(prefix) != std::string::npos) {
          std::cerr << "Matching " << sigName << " with prefix " << prefix
                    << "\n";
          useX = false;
          break;
        }
      }
      inputs.emplace_back(input, pos);
      useXValue.emplace_back(useX);
    }
    pi += input->width;
  }

  for (auto *cell : module->cells()) {
    if (!RTLIL::builtin_ff_cell_types().count(cell->type))
      continue;
    // TODO: filter more unhandled FF types
    assert(cell->type.in("$_DFF_P_") && "Unhandled cell type that is a FF");
    if (cell->type.in("$_DFF_P_")) {
      log("  Found DFF-type cell: %s of type %s\n", log_id(cell),
          log_id(cell->type));

      // FF data output
      auto q = cell->getPort("\\Q");
      std::set<RTLIL::SigBit> outputset = q.to_sigbit_set();
      for (auto b : q.to_sigbit_set()) {
        regs.emplace_back(b);
      }
    }
  }
}

void runXSim(const std::string &filename, RTLIL::Module *module,
             const std::vector<std::string> &controlPrefixes) {
  Abc_Start();

  Abc_Frame_t *pAbc;

  pAbc = Abc_FrameGetGlobalFrame();

  runAbcCommand(pAbc, "read_blif " + filename);
  runAbcCommand(pAbc, "st");

  AigToRTLILObjs aigToYosysObj(module, pAbc, 1000, controlPrefixes);

  xSimulation(aigToYosysObj);

  Abc_Stop();
}

#define XVS0 ABC_INIT_ZERO
#define XVS1 ABC_INIT_ONE
#define XVSX ABC_INIT_DC

static void xSimulation(AigToRTLILObjs helper) {
  Abc_Ntk_t *pNtk = helper.pAbcObj->pNtkCur;
  Abc_Obj_t *pObj;
  int i;
  assert(Abc_NtkIsStrash(pNtk));
  //    srand( 0x12341234 );
  Gia_ManRandom(1);
  abcObjSetXsim(Abc_AigConst1(pNtk), XVS1);

  // Set the value for each Primary input
  Abc_NtkForEachPi(pNtk, pObj, i) {
    if (helper.useXValue[i]) {
      std::cerr << "Using X for PI " << i << "\n";
      abcObjSetXsim(pObj, XVSX);
    } else {
      // If the signal has a control prefix, we simulate using random binary
      // values instead of X.
      std::cerr << "Not using X for PI " << i << "\n";
      abcObjSetXsim(pObj, abcXsimRand2());
    }
  }

  // Initial states of the FFs
  Abc_NtkForEachLatch(pNtk, pObj, i)
      abcObjSetXsim(Abc_ObjFanout0(pObj), Abc_LatchInit(pObj));

  fprintf(stderr, "Frame : Inputs : Latches : Outputs\n");

  for (unsigned f = 0; f < helper.simulationCycles; ++f) {

    Abc_AigForEachAnd(pNtk, pObj, i) {
      abcObjSetXsim(pObj, abcXsimAnd(abcObjGetXsimFanin0(pObj),
                                     abcObjGetXsimFanin1(pObj)));
    }

    Abc_NtkForEachCo(pNtk, pObj, i) {
      abcObjSetXsim(pObj, abcObjGetXsimFanin0(pObj));
    }

    // Dump the current state.
    fprintf(stderr, "%2d : ", f);
    Abc_NtkForEachPi(pNtk, pObj, i) {
      abcXsimPrint(stderr, abcObjGetXsim(pObj));
    }
    fprintf(stderr, " : ");
    Abc_NtkForEachLatch(pNtk, pObj, i) {
      abcXsimPrint(stderr, abcObjGetXsim(Abc_ObjFanout0(pObj)));
    }
    fprintf(stderr, " : ");
    Abc_NtkForEachPo(pNtk, pObj, i) {
      abcXsimPrint(stderr, abcObjGetXsim(pObj));
    }
    std::cerr << "\n";

    // New state inputs
    Abc_NtkForEachPi(pNtk, pObj, i) {
      if (helper.useXValue[i])
        abcObjSetXsim(pObj, XVSX);
      else {
        if (i != (int)helper.resetPinId) {
          abcObjSetXsim(pObj, abcXsimRand2());
        } else {
          std::cerr << "Set reset pin " << i << " to 0\n";
          abcObjSetXsim(pObj, XVS0);
        }
      }
    }

    // transfer the latch values
    Abc_NtkForEachLatch(pNtk, pObj, i) {
      abcObjSetXsim(Abc_ObjFanout0(pObj), abcObjGetXsim(Abc_ObjFanin0(pObj)));
    }
  }

  std::cerr << "Final register states:\n";
  Abc_NtkForEachLatch(pNtk, pObj, i) {

    RTLIL::SigBit regBit = helper.regs[i];
    if (abcObjGetXsim(Abc_ObjFanout0(pObj)) != XVSX) {
      std::cerr << "Latch #" << i << " " << log_id(regBit.wire->name)
                << " at position = " << regBit.offset
                << " is not tainted with x!\n";
    }
    // else {
    //   std::cerr << "Latch #" << i << " " << log_id(regBit.wire->name)
    //             << " at position = " << regBit.offset
    //             << " is tainted with x!\n";
    // }
  }
}

static inline int abcXsimRand2() {
  //    return (rand() & 1) ? XVS1 : XVS0;
  return (rand() & 1) ? XVS1 : XVS0;
}

static inline int abcXsimAnd(int value0, int value1) {
  if (value0 == XVS0 || value1 == XVS0)
    return XVS0;
  if (value0 == XVSX || value1 == XVSX)
    return XVSX;
  assert(value0 == XVS1 && value1 == XVS1);
  return XVS1;
}

static inline void abcObjSetXsim(Abc_Obj_t *pObj, int value) {
  pObj->pCopy = (Abc_Obj_t *)(ABC_PTRINT_T)value;
}

static inline int abcObjGetXsim(Abc_Obj_t *pObj) {
  return (int)(ABC_PTRINT_T)pObj->pCopy;
}

static inline int abcXsimInv(int value) {
  if (value == XVS0)
    return XVS1;
  if (value == XVS1)
    return XVS0;
  assert(value == XVSX);
  return XVSX;
}

static inline int abcObjGetXsimFanin0(Abc_Obj_t *pObj) {
  int retValue;
  retValue = abcObjGetXsim(Abc_ObjFanin0(pObj));
  return Abc_ObjFaninC0(pObj) ? abcXsimInv(retValue) : retValue;
}

static inline int abcObjGetXsimFanin1(Abc_Obj_t *pObj) {
  int retValue;
  retValue = abcObjGetXsim(Abc_ObjFanin1(pObj));
  return Abc_ObjFaninC1(pObj) ? abcXsimInv(retValue) : retValue;
}

static inline void abcXsimPrint(FILE *pFile, int value) {
  if (value == XVS0) {
    fprintf(pFile, "0");
    return;
  }
  if (value == XVS1) {
    fprintf(pFile, "1");
    return;
  }
  assert(value == XVSX);
  fprintf(pFile, "x");
}
