#include "promise/RTLIL/ReplaceDependentFFs.h"
#include "promise/RTLIL/EquipInvariants.h"

#include "kernel/sigtools.h"

#include <iostream>

USING_YOSYS_NAMESPACE

/// Collects the names of the wires (variables) that appear in an expression.
static void collectVariables(const std::shared_ptr<PropAst> &expr,
                             pool<RTLIL::IdString> &variables) {
  std::visit(Overloaded{
                 [&](const int &) {},
                 [&](const RTLIL::IdString &id) { variables.insert(id); },
                 [&](const LNotOp &op) { collectVariables(op.arg, variables); },
                 [&](const auto &op) {
                   collectVariables(op.lhs, variables);
                   collectVariables(op.rhs, variables);
                 },
             },
             *expr);
}

/// Returns the FF cells that drive the bits of \p wire, or an empty vector if
/// some bit is not driven by a `$_DFF_P_` cell.
static std::vector<RTLIL::Cell *>
getDrivingFFs(const SigMap &sigmap,
              const dict<RTLIL::SigBit, RTLIL::Cell *> &ffOfBit,
              RTLIL::Wire *wire) {
  std::vector<RTLIL::Cell *> ffs;
  for (const auto &bit : sigmap(RTLIL::SigSpec(wire))) {
    auto it = ffOfBit.find(bit);
    if (it == ffOfBit.end())
      return {};
    ffs.push_back(it->second);
  }
  return ffs;
}

/// Returns true if a bit of \p target is in the combinational fan-in of the
/// wires \p sources, i.e., reachable backwards through cells that are not
/// FFs.
static bool isInCombinationalFanIn(RTLIL::Module *module, const SigMap &sigmap,
                                   const pool<RTLIL::IdString> &sources,
                                   RTLIL::Wire *target) {
  // The cell that drives each (canonical) bit.
  dict<RTLIL::SigBit, RTLIL::Cell *> driverOfBit;
  for (auto *cell : module->cells())
    for (const auto &[port, sig] : cell->connections())
      if (cell->output(port))
        for (const auto &bit : sigmap(sig))
          driverOfBit[bit] = cell;

  pool<RTLIL::SigBit> targetBits;
  for (const auto &bit : sigmap(RTLIL::SigSpec(target)))
    targetBits.insert(bit);

  // Depth-first search backwards from the source bits; it stops at FFs, whose
  // outputs do not depend combinationally on their inputs.
  std::vector<RTLIL::SigBit> worklist;
  pool<RTLIL::SigBit> visited;
  for (const auto &name : sources)
    for (const auto &bit : sigmap(RTLIL::SigSpec(module->wire(name))))
      worklist.push_back(bit);
  while (!worklist.empty()) {
    RTLIL::SigBit bit = worklist.back();
    worklist.pop_back();
    if (!visited.insert(bit).second)
      continue;
    if (targetBits.count(bit))
      return true;
    auto it = driverOfBit.find(bit);
    if (it == driverOfBit.end() || it->second->type.in(ID($_DFF_P_)))
      continue;
    for (const auto &[port, sig] : it->second->connections())
      if (it->second->input(port))
        for (const auto &inBit : sigmap(sig))
          worklist.push_back(inBit);
  }
  return false;
}

unsigned replaceDependentFFs(RTLIL::Module *module,
                             const std::vector<Invariant> &invariants) {
  unsigned numRemovedFFBits = 0;
  for (const auto &inv : invariants) {
    const auto *eq = std::get_if<EqOp>(inv.ast.get());
    if (!eq)
      continue;

    // The netlist changes with every replacement, so the maps are rebuilt.
    SigMap sigmap(module);
    dict<RTLIL::SigBit, RTLIL::Cell *> ffOfBit;
    for (auto *cell : module->cells())
      if (cell->type.in(ID($_DFF_P_)))
        ffOfBit[sigmap(cell->getPort(ID::Q))[0]] = cell;

    // Try both orientations: `w == e` and `e == w`.
    for (const auto &[wSide, eSide] :
         {std::pair{eq->lhs, eq->rhs}, std::pair{eq->rhs, eq->lhs}}) {
      const auto *wName = std::get_if<RTLIL::IdString>(wSide.get());
      if (!wName)
        continue;
      RTLIL::Wire *w = module->wire(*wName);
      if (!w)
        continue;
      std::vector<RTLIL::Cell *> ffs = getDrivingFFs(sigmap, ffOfBit, w);
      if (ffs.empty())
        continue;

      pool<RTLIL::IdString> eVariables;
      collectVariables(eSide, eVariables);
      // `e` must exist and must not depend on `w` (directly or through
      // combinational logic), otherwise driving `w` by `e` creates a cycle.
      bool allExist = true;
      for (const auto &name : eVariables)
        allExist &= module->wire(name) != nullptr;
      if (!allExist || eVariables.count(*wName) ||
          isInCombinationalFanIn(module, sigmap, eVariables, w))
        continue;

      RTLIL::SigSpec e = instantiateExpression(module, eSide);
      // The invariant guarantees that the value of `e` fits in `w`.
      e.extend_u0(w->width);

      for (auto *ff : ffs)
        module->remove(ff);
      w->attributes.erase(ID::init);
      module->connect(RTLIL::SigSpec(w), e);

      std::cerr << "[INFO] replaced FFs of " << log_id(w) << " (" << w->width
                << " bits) using invariant: " << inv.toString() << "\n";
      numRemovedFFBits += w->width;
      break;
    }
  }
  return numRemovedFFBits;
}
