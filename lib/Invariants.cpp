#include "promise/Invariants.h"
#include "kernel/rtlil.h"
#include "kernel/yosys_common.h"
#include "promise/LinearAlgebra.h"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/bron_kerbosch_all_cliques.hpp>
#include <boost/graph/detail/adjacency_list.hpp>
#include <cstddef>

#include <Eigen/Dense>
#include <Eigen/src/Core/ArithmeticSequence.h>
#include <Eigen/src/Core/Matrix.h>
#include <Eigen/src/Core/MatrixBase.h>

#include <boost/graph/sequential_vertex_coloring.hpp>
#include <boost/range/combine.hpp>
#include <boost/range/detail/combine_cxx11.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <sys/wait.h>

using Graph =
    boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS>;
using Vertex = boost::graph_traits<Graph>::vertex_descriptor;
using Vertex_size_t = boost::graph_traits<Graph>::vertices_size_type;
using vertex_index_map =
    boost::property_map<Graph, boost::vertex_index_t>::const_type;

std::vector<Invariant>
inferLinearEqualities(RTLIL::Module *module, const Eigen::MatrixXi &matrix,
                      const std::vector<RTLIL::IdString> &signals) {

  // We also want to infer the constant term: to do this, we append a constant
  // one column to the right side of the matrix.
  Eigen::VectorXi const1 = Eigen::VectorXi::Ones(matrix.rows());
  Eigen::MatrixXi matrixWithConst1(matrix.rows(), matrix.cols() + 1);
  matrixWithConst1 << matrix, const1;

  std::vector<Eigen::VectorXi> basis =
      computeBasisOfNullSpace(matrixWithConst1);

  std::vector<Invariant> properties;

  auto makeTerm = [](int coeff, RTLIL::IdString id) {
    auto varAst = mkAst(id);
    if (coeff > 1) {
      auto coeffAst = mkAst(coeff);
      return mkAst(MulOp(coeffAst, varAst));
    }
    return varAst;
  };

  auto sumTerms = [](const std::vector<std::shared_ptr<PropAst>> &terms) {
    std::shared_ptr<PropAst> ast;
    if (terms.empty()) {
      return mkAst(0);
    }
    ast = terms[0];
    for (size_t j = 1; j < terms.size(); ++j) {
      ast = mkAst(AddOp(ast, terms[j]));
    }
    return ast;
  };

  for (const auto &vec : basis) {
    std::vector<std::shared_ptr<PropAst>> lhsTerms;
    std::vector<std::shared_ptr<PropAst>> rhsTerms;
    for (size_t i = 0; i < signals.size(); ++i) {
      if (vec(i) > 0) {
        lhsTerms.push_back(makeTerm(vec[i], signals[i]));
      } else if (vec(i) < 0) {
        rhsTerms.push_back(makeTerm(-vec[i], signals[i]));
      }
    }

    auto constTerm = vec(/*last element in the basis vector*/
                         signals.size());

    if (constTerm > 0) {
      lhsTerms.push_back(mkAst(constTerm));
    } else if (constTerm < 0) {
      rhsTerms.push_back(mkAst(-constTerm));
    }

    auto lhsAst = sumTerms(lhsTerms);
    auto rhsAst = sumTerms(rhsTerms);

    properties.emplace_back(mkAst(EqOp(lhsAst, rhsAst)));
  }

  return properties;
}

using CompatibleNodes = std::vector<size_t>;

std::vector<CompatibleNodes>
getMutexSignalsViaGreedyGraphColoring(const Graph &signalConflictGraph) {

  std::vector<size_t> colorVec(boost::num_vertices(signalConflictGraph), 0);
  auto indexMap = boost::get(boost::vertex_index, signalConflictGraph);
  auto colorMap = make_safe_iterator_property_map(colorVec.begin(),
                                                  colorVec.size(), indexMap);
  auto numColors =
      boost::sequential_vertex_coloring(signalConflictGraph, colorMap);

  std::vector<std::vector<size_t>> nodesPerColor(numColors);

  // Group vertices by their color
  for (size_t v = 0; v < colorVec.size(); ++v) {
    size_t c = colorVec[v];
    nodesPerColor[c].push_back(v);
  }
  return nodesPerColor;
}

struct CompatibleSignalsCollector {
  std::vector<CompatibleNodes> &cliques;

  template <typename VertexSetTy>
  void clique(const VertexSetTy &cliqueIter, const Graph &g) {
    CompatibleNodes clique;

    for (typename VertexSetTy::const_iterator i = cliqueIter.begin();
         i != cliqueIter.end(); ++i) {
      size_t node = *i;
      clique.push_back(node);
    }
    cliques.push_back(clique);
    assert(!clique.empty());
  }

  CompatibleSignalsCollector(std::vector<CompatibleNodes> &cliques)
      : cliques(cliques){};
};

std::vector<CompatibleNodes>
getMutexSignalsViaCliqueEnumeration(const Graph &signalConflictGraph) {

  typedef boost::graph_traits<Graph>::vertex_descriptor Vertex;

  Graph signalCompatGraph(signalConflictGraph.vertex_set().size());

  // Build the compatibility graph:
  // If there are no edge between two nodes in the conflict graph, then we add
  // an edge in the compatibility graph
  for (Vertex u = 0; u < boost::num_vertices(signalConflictGraph); ++u) {
    for (Vertex v = 0; v < boost::num_vertices(signalConflictGraph); ++v) {
      if (!boost::edge(u, v, signalConflictGraph).second) {
        boost::add_edge(u, v, signalCompatGraph);
      }
    }
  }

  std::vector<CompatibleNodes> cliques;
  CompatibleSignalsCollector collector(cliques);

  boost::bron_kerbosch_all_cliques(signalCompatGraph, collector, 2);

  return cliques;
}

std::pair<Eigen::MatrixXi, std::vector<IdString>>
pickSingleBitSignals(Module *module, const Eigen::MatrixXi &mat,
                     const std::vector<IdString> &signalList) {

  assert(mat.cols() == signalList.size());
  std::vector<size_t> indices;

  std::vector<IdString> newSignals;

  for (size_t i = 0; i < signalList.size(); ++i) {
    if (module->wire(signalList[i])->width == 1) {
      indices.push_back(i);
      newSignals.push_back(signalList[i]);
    }
  }
  Eigen::MatrixXi newMatrix(mat.rows(), indices.size());

  for (size_t i = 0; i < indices.size(); ++i) {
    newMatrix.col(i) = mat.col(indices[i]);
  }
  assert(newMatrix.cols() == newSignals.size());
  return std::make_pair(newMatrix, newSignals);
}

std::vector<Invariant> inferLinearInequalitiesViaConflictGraph(
    RTLIL::Module *module, const Eigen::MatrixXi &matrix,
    const std::vector<RTLIL::IdString> &signals) {

  auto [singleBitMatrix, singleBitSignals] =
      pickSingleBitSignals(module, matrix, signals);

  if (static_cast<std::size_t>(matrix.cols()) != signals.size()) {
    throw std::runtime_error("The number of columns do not match the signals");
  }

  // We add one node for each signal and its completment
  size_t offset = singleBitSignals.size();
  Graph signalConflictGraph(offset * 2);

  // Append the singleBitSignals with their complements

  using WireWithIsNegatedType = std::pair<IdString, /*isNegated=*/bool>;

  std::vector<WireWithIsNegatedType> sigNameWithIsNegated;
  sigNameWithIsNegated.reserve(singleBitSignals.size() * 2);

  for (const auto &sig : singleBitSignals) {
    assert(module->wire(sig) && module->wire(sig)->width == 1);
    sigNameWithIsNegated.emplace_back(sig, false);
  }
  for (const auto &sig : singleBitSignals) {
    sigNameWithIsNegated.emplace_back(sig, true);
  }

  for (size_t i = 0; i < offset; i++) {
    // Each signal and its complement must have different colors (to
    // eliminate trivial relations like a + ~a <= 1).
    boost::add_edge(i, i + offset, signalConflictGraph);
  }

  Eigen::MatrixXi matrixConcat(singleBitMatrix.rows(), offset * 2);

  auto matrixNegate = 1 - singleBitMatrix.array();

  matrixConcat << singleBitMatrix, matrixNegate;

  for (size_t sigI = 0; sigI < 2 * offset; ++sigI) {
    for (size_t sigJ = sigI + 1; sigJ < 2 * offset; ++sigJ) {
      Eigen::VectorXi sumCol = matrixConcat.col(sigI) + matrixConcat.col(sigJ);
      if (!((sumCol.array() <= 1).all())) {
        boost::add_edge(sigI, sigJ, signalConflictGraph);
      }
    }
  }

  // For a given identifier name id, returns an ast that is either representing
  // the signal itself (id) or its complement (!id).
  auto makeTerm = [](bool isNegated, RTLIL::IdString id) {
    auto varAst = mkAst(id);
    if (isNegated) {
      return mkAst(LNotOp(varAst));
    }
    return varAst;
  };

  auto sumTerms = [](const std::vector<std::shared_ptr<PropAst>> &terms) {
    std::shared_ptr<PropAst> ast;
    if (terms.empty()) {
      return mkAst(0);
    }
    ast = terms[0];
    for (size_t j = 1; j < terms.size(); ++j) {
      ast = mkAst(AddOp(ast, terms[j]));
    }
    return ast;
  };

  std::vector<CompatibleNodes> nodesPerColor =
      getMutexSignalsViaGreedyGraphColoring(signalConflictGraph);

  // NOTE: Enumerate all maximal cliques is extremely time consuming!
  // std::vector<CompatibleNodes> nodesPerColor =
  //     getMutexSignalsViaCliqueEnumeration(signalConflictGraph);

  std::vector<Invariant> properties;

  for (const auto &compatibleSignals : nodesPerColor) {
    if (compatibleSignals.size() > 1) {
      std::vector<std::shared_ptr<PropAst>> terms;
      for (size_t id : compatibleSignals) {
        auto [wire, isNegated] = sigNameWithIsNegated[id];
        terms.push_back(makeTerm(isNegated, wire));
      }
      auto invAst = mkAst(LeOp(sumTerms(terms), mkAst(1)));
      properties.emplace_back(invAst);
    }
  }

  return properties;
}
