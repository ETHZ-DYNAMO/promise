#include "promise/RTLIL/EquipInvariants.h"
#include "kernel/rtlil.h"
#include "kernel/yosys_common.h"
#include "promise/Invariants.h"
#include <csignal>
#include <stdexcept>
#include <utility>

void zeroExtend(RTLIL::SigSpec &sig, int targetWidth) {
  int currentWidth = GetSize(sig);
  if (currentWidth >= targetWidth)
    sig.extract(0, targetWidth); // truncate if wider

  sig.append(RTLIL::SigSpec(RTLIL::State::S0, targetWidth - currentWidth));
}

/// \brief: This class instruments a given RTLIL module with the invariant
/// described in AST. It recursively traverses the AST and builds operation from
/// the AST node. The returned SigSpec construct will be picked up by the parent
/// calls to generate operations.

struct InstantiateInvariantVisitor {
  RTLIL::Module *module;

  using SigSpecAndMaxValType = std::pair<SigSpec, unsigned>;

  SigSpecAndMaxValType operator()(const int &n) const {
    assert(n >= 0 && "This method assumes that we do unsigned arithmetic.");
    return std::make_pair(SigSpec(RTLIL::Const(n, ceil_log2(1 + n))), n);
  };

  SigSpecAndMaxValType operator()(const RTLIL::IdString &n) const {
    auto *w = module->wire(n);
    assert(w && "n is not a wire in the module!");
    return std::make_pair(SigSpec(w), (1 << w->width) - 1);
  }

  SigSpecAndMaxValType operator()(const LNotOp &n) const {
    auto [arg, _] = visit(n.arg);
    // Create the unary operator according to the type.
    assert(arg.size() == 1 &&
           "Attempting to instantiate a logical NOT on a word!");
    RTLIL::Wire *out = module->addWire(NEW_ID, 1);
    module->addNot(NEW_ID, arg, SigSpec(out));
    return std::make_pair(SigSpec(out), 1);
  }

  SigSpecAndMaxValType operator()(const AddOp &op) const {
    auto [lhs, lMaxVal] = visit(op.lhs);
    auto [rhs, rMaxVal] = visit(op.rhs);
    unsigned width = max(lhs.size(), rhs.size());
    zeroExtend(lhs, width);
    zeroExtend(rhs, width);
    RTLIL::Wire *out =
        module->addWire(NEW_ID, ceil_log2(1 + lMaxVal + rMaxVal));
    module->addAdd(NEW_ID, lhs, rhs, SigSpec(out));
    return std::make_pair(SigSpec(out), lMaxVal + rMaxVal);
  }

  SigSpecAndMaxValType operator()(const MulOp &op) const {
    auto [lhs, lMaxVal] = visit(op.lhs);
    auto [rhs, rMaxVal] = visit(op.rhs);
    RTLIL::Wire *out =
        module->addWire(NEW_ID, ceil_log2(1 + lMaxVal * rMaxVal));
    module->addMul(NEW_ID, lhs, rhs, SigSpec(out));
    return std::make_pair(SigSpec(out), lMaxVal * rMaxVal);
  }

  SigSpecAndMaxValType operator()(const ImpliesOp &) const {
    assert(false && "Not implemented!");
  }

  SigSpecAndMaxValType operator()(const LeOp &op) const {
    auto [lhs, lMaxVal] = visit(op.lhs);
    auto [rhs, rMaxVal] = visit(op.rhs);
    unsigned width = max(lhs.size(), rhs.size());
    zeroExtend(lhs, width);
    zeroExtend(rhs, width);
    RTLIL::Wire *out = module->addWire(NEW_ID, 1);
    module->addLe(NEW_ID, lhs, rhs, SigSpec(out));
    return std::make_pair(SigSpec(out), 1);
  }

  SigSpecAndMaxValType operator()(const EqOp &op) const {
    auto [lhs, lMaxVal] = visit(op.lhs);
    auto [rhs, rMaxVal] = visit(op.rhs);
    unsigned width = max(lhs.size(), rhs.size());
    zeroExtend(lhs, width);
    zeroExtend(rhs, width);
    RTLIL::Wire *out = module->addWire(NEW_ID, 1);
    module->addEq(NEW_ID, lhs, rhs, SigSpec(out));
    return std::make_pair(SigSpec(out), 1);
  }

  SigSpecAndMaxValType visit(const std::shared_ptr<PropAst> &n) const {
    // Remark for future visitors: the c++ compiler complains if the visitor
    // struct has not implemented "operator()" for all types in the variant.
    return std::visit(*this, *n);
  }
};

/// \brief: This function instruments the module with a linear invariant, and
/// returns a SigSpec of the created output pin.
///
/// \note: Convention: output = 1 means good, output = 0 mean bad (opposite from
/// ABC convention, but more intuitive).
SigSpec instrumentSingleInvariant(RTLIL::Module *m, const Invariant &inv,
                                  const std::string &name) {

  InstantiateInvariantVisitor visitor{m};
  auto [invLogicOutput, _] = visitor.visit(inv.ast);
  m->rename(invLogicOutput.as_wire(), RTLIL::escape_id(name));
  return invLogicOutput;
}

void instrumentInvariants(RTLIL::Module *m,
                          const std::vector<Invariant> &invariants,
                          bool trimOriginalOutputs, bool separateInvariants) {

  RTLIL::SigSpec invPins;
  for (size_t invId = 0; invId < invariants.size(); invId++) {
    RTLIL::SigSpec sig = instrumentSingleInvariant(
        m, invariants[invId], "inv_" + std::to_string(invId));
    invPins.append(sig);
  }

  if (trimOriginalOutputs) {
    for (auto *wire : m->wires()) {
      wire->port_output = false;
    }
  }

  if (invariants.empty()) {
    std::cerr << "Warning - instrumenting an empty invariant (i.e., trivially true)!\n";
    invPins.append(RTLIL::Const(1));
  }

  if (separateInvariants) {
    RTLIL::Wire *propertyPin =
        m->addWire(RTLIL::escape_id("property_pin"), invPins.size());
    // bit-wise invert all the invariant pins, and declare it as PO
    m->addNot(NEW_ID, invPins, propertyPin);
    propertyPin->port_output = true;
  } else {
    RTLIL::Wire *invertedPropertyPin = m->addWire(NEW_ID, 1);
    m->addReduceAnd(NEW_ID, invPins, SigSpec(invertedPropertyPin));
    RTLIL::Wire *propertyPin = m->addWire(RTLIL::escape_id("property_pin"), 1);
    m->addNot(NEW_ID, SigSpec(invertedPropertyPin), SigSpec(propertyPin));
    propertyPin->port_output = true;
  }

  m->fixup_ports();
  m->sort();
}
