//===-- FPRewritingSolver.cpp ---------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Solver.h"

#include "klee/Expr.h"
#include "klee/Constraints.h"
#include "klee/SolverImpl.h"

#include "klee/util/ExprUtil.h"

#include <map>
#include <vector>
#include <ostream>
#include <iostream>

using namespace klee;
using namespace llvm;


class FPRewritingSolver : public SolverImpl {
private:
  Solver *solver;

public:
  FPRewritingSolver(Solver *_solver) 
    : solver(_solver) {}
  ~FPRewritingSolver() { delete solver; }

  bool computeTruth(const Query&, bool &isValid);
  bool computeValidity(const Query&, Solver::Validity &result);
  bool computeValue(const Query&, ref<Expr> &result);
  bool computeInitialValues(const Query& query,
                            const std::vector<const Array*> &objects,
                            std::vector< std::vector<unsigned char> > &values,
                            bool &hasSolution);
};
  
bool FPRewritingSolver::computeTruth(const Query &q, bool &isValid) {
  puts("FPRewritingSolver::computeTruth called");
  return solver->impl->computeTruth(q, isValid);
}

bool FPRewritingSolver::computeValidity(const Query &q, Solver::Validity &result) {
  puts("FPRewritingSolver::computeValidity called");
  return solver->impl->computeValidity(q, result);
}

bool FPRewritingSolver::computeValue(const Query &q, ref<Expr> &result) {
  puts("FPRewritingSolver::computeValue called");
  return solver->impl->computeValue(q, result);
}

bool FPRewritingSolver::computeInitialValues(const Query& query,
                          const std::vector<const Array*> &objects,
                          std::vector< std::vector<unsigned char> > &values,
                          bool &hasSolution) {
  puts("FPRewritingSolver::computeInitialValues called");
  return solver->impl->computeInitialValues(query, objects, values,
                                            hasSolution);
}

Solver *klee::createFPRewritingSolver(Solver *s) {
  return new Solver(new FPRewritingSolver(s));
}
