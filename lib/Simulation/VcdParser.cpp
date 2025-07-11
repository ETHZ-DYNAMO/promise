#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "promise/Simulation/VcdParser.h"
#include "promise/StringUtils.h"

#include <Eigen/Dense>
#include <Eigen/src/Core/ArithmeticSequence.h>
#include <Eigen/src/Core/Matrix.h>
#include <Eigen/src/Core/MatrixBase.h>

// Yosys headers
#include "kernel/log.h"
#include "kernel/rtlil.h"
#include "kernel/yosys_common.h"

USING_YOSYS_NAMESPACE

// Extract a set of unique states from a VCD file for a given list of signals
// (Wire* in Yosys).
Eigen::MatrixXi
vcdToSignalMatrix(RTLIL::Module *module, // NOLINT
                  const std::string &vcdPath,
                  const std::vector<RTLIL::IdString> &signalList) {

  std::set<RTLIL::IdString> signalsToTrack;

  for (const auto &sig : signalList) {
    assert(module->wire(sig));
    signalsToTrack.insert(sig);
  }

  std::ifstream vcd(vcdPath);
  if (!vcd.is_open())
    throw std::runtime_error("Failed to open file: " + vcdPath);

  // symbol -> [signal names]
  std::map<std::string, std::vector<RTLIL::IdString>> symbolToNames;

  using sigToValueMap = std::map<RTLIL::IdString, std::string>;

  // signal name -> last value
  sigToValueMap lastValues;

  // A unique set of states
  std::set<sigToValueMap> uniqueStates;

  std::string line;

  // Phase 1: Parse signal definitions
  while (std::getline(vcd, line)) {
    if (line.find("$enddefinitions") != std::string::npos)
      break;

    std::istringstream iss(line);
    std::string word;
    iss >> word;

    if (word == "$var") {
      std::string type, width, symbol, name;
      iss >> type >> width >> symbol;
      std::getline(iss, name);
      name = trim(name.substr(0, name.find(" $end")));
      name = name.substr(0, name.find_first_of(" \t\n\r"));

      RTLIL::IdString sigNameId = RTLIL::escape_id(name);

      // Only track if this name is one of the signals requested
      if (signalsToTrack.count(sigNameId)) {
        symbolToNames[symbol].emplace_back(sigNameId);
        lastValues[sigNameId] = "x"; // unknown default
      }
    }
  }

  std::string currentTime = "0";
  bool timeChanged = false;

  // Phase 2: Parse value changes
  while (std::getline(vcd, line)) {
    if (line.empty())
      continue;

    if (line[0] == '#') {
      if (timeChanged) {
        uniqueStates.insert(lastValues);
      }
      currentTime = line.substr(1);
      timeChanged = true;
    } else {
      if (line[0] == 'b') {
        // multi-bit
        std::istringstream iss(line);
        std::string value, symbol;
        iss >> value >> symbol;
        if (symbolToNames.count(symbol)) {
          std::string rawVal = value.substr(1); // remove 'b'
          for (const auto &name : symbolToNames[symbol]) {
            lastValues[name] = rawVal;
          }
        }
      } else {
        // single-bit
        char val = line[0];
        std::string symbol = line.substr(1);
        if (symbolToNames.count(symbol)) {
          for (const auto &name : symbolToNames[symbol]) {
            lastValues[name] = std::string(1, val);
          }
        }
      }
    }
  }

#ifdef PROMISE_LOG
  // Write final row
  for (const auto &sig : signalList) {
    std::cerr << log_id(sig) << ",";
  }
  std::cerr << "\n";

  for (sigToValueMap state : uniqueStates) {
    for (const auto &sig : signalList) {
      std::cerr << state[log_id(sig)] << ",";
    }
    std::cerr << "\n";
  }
#endif

  // Preallocate matrix
  Eigen::MatrixXi mat(/* x = */ uniqueStates.size(),
                      /* y = */ signalList.size());

  unsigned row = 0;
  for (sigToValueMap state : uniqueStates) {
    unsigned col = 0;
    for (const auto &sig : signalList) {
      mat(row, col) = static_cast<uint32_t>(std::stoul(state[sig], nullptr, 2));
      ++col;
    }
    ++row;
  }

  return mat;
}