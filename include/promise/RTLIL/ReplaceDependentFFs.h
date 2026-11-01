#pragma once

#include "kernel/rtlil.h"
#include "kernel/yosys_common.h"
#include "promise/Invariants.h"

#include <vector>

/// \brief Replaces FFs by combinational logic using proven equality
/// invariants, i.e., removes dependent state elements (Case et al., FMCAD'08,
/// "Invariant-Strengthened Elimination of Dependent State Elements").
///
/// An invariant `w == e` (or `e == w`) is used when `w` is a single wire whose
/// bits are all driven by FFs and `e` does not depend on `w`. The FFs that
/// drive `w` are removed and `w` is driven by the logic of `e` instead.
/// Candidates are taken greedily; one is skipped if `w` is in the
/// combinational fan-in of `e` (in the netlist after the previous
/// replacements), since replacing it would create a combinational cycle.
///
/// Unlike in FMCAD'08, no initial-state correction is needed: the invariants
/// hold in every reachable state, including the initial one, so `e` evaluates
/// to the initial value of `w` in the initial state.
///
/// \param module: the module; it is modified in place. Assumes that the FFs
/// are `$_DFF_P_` cells (after `synth; dffunmap`).
/// \param invariants: invariants that hold in all reachable states (proven)
/// \returns the number of removed FF bits
unsigned replaceDependentFFs(RTLIL::Module *module,
                             const std::vector<Invariant> &invariants);
