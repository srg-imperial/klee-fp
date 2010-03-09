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

  Query rewriteQuery(const Query &q);
  ref<Expr> rewriteConstraint(ref<Expr> e);

  bool computeTruth(const Query&, bool &isValid);
  bool computeValidity(const Query&, Solver::Validity &result);
  bool computeValue(const Query&, ref<Expr> &result);
  bool computeInitialValues(const Query& query,
                            const std::vector<const Array*> &objects,
                            std::vector< std::vector<unsigned char> > &values,
                            bool &hasSolution);
};

ref<Expr> FPRewritingSolver::rewriteConstraint(ref<Expr> e) {
  return e;
}

Query FPRewritingSolver::rewriteQuery(const Query &q) {
  std::vector< ref<Expr> > constraints;
  for (ConstraintManager::constraint_iterator i  = q.constraints.begin();
                                              i != q.constraints.end();
                                            ++i) {
    constraints.push_back(rewriteConstraint(*i));
  }
  return Query(ConstraintManager(constraints), rewriteConstraint(q.expr));
}

bool FPRewritingSolver::computeTruth(const Query &q, bool &isValid) {
  return solver->impl->computeTruth(rewriteQuery(q), isValid);
}

bool FPRewritingSolver::computeValidity(const Query &q, Solver::Validity &result) {
  return solver->impl->computeValidity(rewriteQuery(q), result);
}

bool FPRewritingSolver::computeValue(const Query &q, ref<Expr> &result) {
  return solver->impl->computeValue(rewriteQuery(q), result);
}

bool FPRewritingSolver::computeInitialValues(const Query& query,
                          const std::vector<const Array*> &objects,
                          std::vector< std::vector<unsigned char> > &values,
                          bool &hasSolution) {
  return solver->impl->computeInitialValues(rewriteQuery(query), objects, values,
                                            hasSolution);
}

Solver *klee::createFPRewritingSolver(Solver *s) {
  return new Solver(new FPRewritingSolver(s));
}
