#include "promise/ModelCheckingResult.h"
#include "kernel/rtlil.h"
#include "promise/RTLIL/RTLILUtils.h"
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>

void expectLine(std::ifstream &is, std::string &line) {
  if (!std::getline(is, line))
    throw std::runtime_error("Logfile is ill-formed!");
}

/// \brief convert the input string "bits" to a bitset\
///
/// \IMPORTANT: bits[0] is the LSB and bits[bits.size() - 1] is the MSB. This is
/// the opposite of the built-in initialize dynamic_bitset<>(string)
boost::dynamic_bitset<> strToBits(std::string bits) {
  auto bitset = boost::dynamic_bitset<>(bits.size());
  for (size_t i = 0; i < bits.size(); i++) {
    bitset[i] = bits[i] == '1';
  }
  return bitset;
}

void dispatchLine(
    const std::string &line, const std::vector<Wire *> &wires,
    std::map<RTLIL::IdString, std::vector<boost::dynamic_bitset<>>>
        &sigToValueMap) {

  unsigned pos = 0;

  for (auto *wire : wires) {
    auto substrWire = line.substr(pos, wire->width);
    // Yosys dump input the blif in increasing indices, e.g.,
    // arr[0] arr[1] arr[2] ...
    // To recover the HEX value, we need to reverse it:
    // std::reverse(substrWire.begin(), substrWire.end());
    auto bitset = strToBits(substrWire);

    sigToValueMap[wire->name].push_back(bitset);
    pos += wire->width;
  }
  assert(pos == line.size());
}

ModelCheckingResult
ModelCheckingResult::parseAbcLogFile(RTLIL::Module *module,
                                     const std::string &logFile) {

  ModelCheckingResult result;

  std::vector<Wire *> inputWires = getSortedInput(module);

  for (const auto &w : inputWires) {
    result.inputValues[w->name] = {};
  }

  if (inputWires.empty()) {
    throw std::runtime_error("No input signals configured!");
  }

  std::ifstream isLog(logFile);
  if (!isLog.is_open()) {
    throw std::runtime_error("Logfile cannot be opened!");
  }

  std::string line;

  // An example of the CEX from the pdr command in ABC:
  // snl_SAT 4 pdr 0 5     <--- this is always the first line
  // 000000                <--- value of the initial state
  // 001000000000000000    <--- numPIs * numCexStates

  // Parsing the header: snl_<STATUS> ...
  expectLine(isLog, line);
  auto tokens = split(line, ' ');

  if (tokens[0] == ABC_SAFE) {
    result.status = SAFE;
    return result;
  }

  if (tokens[0] == ABC_UNSAFE) {
    result.status = UNSAFE;
  } else
    throw std::runtime_error("Unknown problem in ABC " + line);

  // If there is a counterexample, then the last token in the line indicates how
  // many states are in the CEX trace
  //
  // Example (5 states in this case):
  // snl_SAT 4 pdr 0 5
  assert(tokens.size() == 5);
  result.numCexStates = std::stoi(tokens[tokens.size() - 1]) + 1;

  unsigned numInputBits = 0;
  for (const auto &s : inputWires) {
    assert(s->width <= 32);
    numInputBits += s->width;
  }

  // Parsing and skipping the initial state:
  expectLine(isLog, line);

  // Parsing the CEX:
  expectLine(isLog, line);

  // ABC dumps the entire CEX in a single line: so we need to calculate the bit
  // position for each state
  for (unsigned i = 0; i < result.numCexStates * numInputBits;
       i += numInputBits) {
    dispatchLine(line.substr(i, numInputBits), inputWires, result.inputValues);
  }

  isLog.close();
  return result;
}

ModelCheckingResult
ModelCheckingResult::parserIC3LogFile(RTLIL::Module *module,
                                      const std::string &logFile) {

  ModelCheckingResult result;

  std::vector<Wire *> inputWires = getSortedInput(module);

  for (const auto &w : inputWires) {
    result.inputValues[w->name] = {};
  }

  std::ifstream isLog(logFile);
  std::cerr << "Paring CEX file: " << logFile << "\n";
  if (!isLog.is_open()) {
    throw std::runtime_error("Logfile cannot be opened!");
  }

  std::string line;

  // An example of the CEX from rIC3:
  //
  // RESULT: SAT <--- we start parsing the logfile when we see this line
  // 1
  // b0
  // 000000 <--- value of the FFs in their initial state
  // 101    <--- the PIs' values of each state in the CEX trace
  // 101    <--- the PIs' values of each state in the CEX trace
  // 101    <--- the PIs' values of each state in the CEX trace
  // .      <--- the trace ends here

  // Parsing the header: snl_<STATUS> ...
  while (std::getline(isLog, line)) {
    if (line.std::string::starts_with("RESULT")) {
      auto tokens = split(line, ' ');
      if (tokens.size() < 2) {
        throw std::runtime_error("The rIC3 counterexample is ill-formed!");
      }
      //
      if (tokens[1] == RIC3_SAFE) {
        result.status = SAFE;
        return result;
      }

      // Parse the counterexample now:
      if (tokens[1] == RIC3_UNSAFE) {
        result.status = UNSAFE;
      } else {
        throw std::runtime_error("The rIC3 counterexample is ill-formed");
      }

      // The output [?] values (need to fact check)
      expectLine(isLog, line);
      assert(split(line, ' ')[0] == "1");

      // The output [?] position (need to fact check)
      expectLine(isLog, line);
      assert(split(line, ' ')[0] == "b0");

      // The initial state of the registers
      expectLine(isLog, line);

      while (std::getline(isLog, line)) {
        if (line.std::string::starts_with(".")) {
          break;
        }

        dispatchLine(line, inputWires, result.inputValues);
        result.numCexStates += 1;
      }
      return result;
    }
  }
  throw std::runtime_error("The rIC3 counterexample is ill-formed");
}