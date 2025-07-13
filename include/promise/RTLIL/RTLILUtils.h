#pragma once
#include "kernel/rtlil.h"
#include "kernel/yosys_common.h"
#include <cassert>

USING_YOSYS_NAMESPACE

/// \brief: Given an RTLIL module, this function returns the all the output
/// wires of registers.
///
/// \note: It only deals with simple "$_DFF_P_" registers
///
/// \note: Maybe it is better to return vector<IdString> instead of Wire* to be
/// portable across duplicated designs?
inline std::vector<Wire *> getRegOutputs(RTLIL::Module *m) {
  // The logic below attempts to get all bits driven by the Q output of a clock
  // std::set<RTLIL::SigBit> clockedBits;
  std::vector<RTLIL::Wire *> regOuts;
  for (auto *cell : m->cells()) {
    if (!RTLIL::builtin_ff_cell_types().count(cell->type))
      continue;
    // TODO: filter more unhandled FF types
    assert(cell->type.in("$_DFF_P_") && "Unhandled cell type that is a FF");
    if (cell->type.in("$_DFF_P_")) {
      log("  Found DFF-type cell: %s of type %s\n", log_id(cell),
          log_id(cell->type));

      // FF output
      auto q = cell->getPort("\\Q");
      std::set<RTLIL::SigBit> outputset = q.to_sigbit_set();
      for (auto b : q.to_sigbit_set()) {
        regOuts.push_back(b.wire);
      }
    }
  }
  return regOuts;
}

// This returns the "Cell *" that has an output that drives the Wire * w
// [Quirk in Yosys]: Somehow in a cloned design, the "driverCell_" is not
// preserved, so we cannot directly use wire->driverCell().
inline RTLIL::Cell *getDriverCell(Module *module, RTLIL::Wire *wire) {
  for (auto *cell : module->cells()) {
    for (auto [portIdentifier, sigSpec] : cell->connections()) {
      // NOTE: as_wire() does not check if SigSpec was actually from a wire!
      if (cell->output(portIdentifier) && (sigSpec.is_wire()) &&
          (sigSpec.as_wire() == wire)) {
        return cell;
      }
    }
  }
  return nullptr;
}

// This function returns a vector of the given module sorted according to the
// port ID.
// NOTE: the RTLIL::Module::sort() function only sorts the set of cells accoding
// to the IdString, but it will not sort the module in ascending order.
inline std::vector<Wire *> getSortedInput(Module *module) {
  // Make sure that all the ports have their ID assigned.
  module->fixup_ports();
  std::vector<Wire *> inputs;
  for (auto *wire : module->wires()) {
    if (wire->port_input) {
      inputs.push_back(wire);
    }
  }
  std::sort(inputs.begin(), inputs.end(),
            [](Wire *a, Wire *b) { return a->port_id < b->port_id; });
  return inputs;
}

/// \brief: This function checks if all the FFs in a given RTLIL module are
/// intitialized.
bool checkFFInitialized(Module *module, Cell *ff);