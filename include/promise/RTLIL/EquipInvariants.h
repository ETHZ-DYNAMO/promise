#pragma once
#include "kernel/rtlil.h"
#include "kernel/yosys_common.h"
#include "promise/Invariants.h"

/// \brief Adds a list of invariants to the POs of an RTLIL module
///
/// \param m: RTLIL module
/// \param invariants: a list of invariants to be proven / used for sequential
/// synthesis
/// \param trimOriginalOutputs: remove the original POs from the module
/// \param separateInvariants: separate invariant pins
void instrumentInvariants(RTLIL::Module *m,
                          const std::vector<Invariant> &invariants,
                          bool trimOriginalOutputs = true,
                          bool separateInvariants = false);

/// \brief Adds the logic that computes an (unsigned) arithmetic expression of
/// an invariant (e.g., one side of an equality) to the module.
///
/// \param m: RTLIL module that contains the wires of the expression
/// \param expr: the expression; it must not contain comparisons
/// \returns the signal that holds the value of the expression
RTLIL::SigSpec instantiateExpression(RTLIL::Module *m,
                                     const std::shared_ptr<PropAst> &expr);