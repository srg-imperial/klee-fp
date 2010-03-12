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

  ref<Expr> constrainEquality(ref<Expr> lhs, ref<Expr> rhs);

  ref<Expr> rewriteConstraint(const ref<Expr> &e);
  const ConstraintManager *rewriteConstraints(const ConstraintManager &cm, bool &changed);

  bool computeTruth(const Query&, bool &isValid);
  bool computeValidity(const Query&, Solver::Validity &result);
  bool computeValue(const Query&, ref<Expr> &result);
  bool computeInitialValues(const Query& query,
                            const std::vector<const Array*> &objects,
                            std::vector< std::vector<unsigned char> > &values,
                            bool &hasSolution);
};

/* Given a pair of floating point expressions lhs and rhs, return an expression
 * which is a sufficient condition for either of:
 *
 *   1. lhs == rhs (ordered comparison) holds
 *   2. both lhs and rhs are NaN
 *
 * The inverse of the expression is also a necessary condition for
 * lhs != rhs (ordered comparison) to hold
 */
ref<Expr> FPRewritingSolver::constrainEquality(ref<Expr> lhs, ref<Expr> rhs) {
  Expr::Kind kind = lhs->getKind();
  if (kind != rhs->getKind())
    return IConstantExpr::alloc(0, Expr::Bool);

  switch (kind) {
    case Expr::FConstant: {
      const APFloat &flhs = cast<FConstantExpr>(lhs)->getAPValue(), &frhs = cast<FConstantExpr>(rhs)->getAPValue();
      bool cond = flhs.bitwiseIsEqual(frhs) || flhs.isZero() && frhs.isZero();
      return IConstantExpr::alloc(cond, Expr::Bool);
    }
    case Expr::FAdd:
    case Expr::FMul:
      return OrExpr::create(
               AndExpr::create(constrainEquality(lhs->getKid(0), rhs->getKid(0)),
                               constrainEquality(lhs->getKid(1), rhs->getKid(1))),
               AndExpr::create(constrainEquality(lhs->getKid(0), rhs->getKid(1)),
                               constrainEquality(lhs->getKid(1), rhs->getKid(0))));
    case Expr::FSub:
    case Expr::FDiv:
    case Expr::FRem:
      return AndExpr::create(constrainEquality(lhs->getKid(0), rhs->getKid(0)),
                             constrainEquality(lhs->getKid(1), rhs->getKid(1)));
    case Expr::FPExt:
    case Expr::FPTrunc: {
      ref<Expr> lhsKid = lhs->getKid(0), rhsKid = rhs->getKid(0);
      if (lhsKid->asFPExpr()->getSemantics() != rhsKid->asFPExpr()->getSemantics())
        return IConstantExpr::alloc(0, Expr::Bool);
      return constrainEquality(lhsKid, rhsKid);
    }
    case Expr::UIToFP:
    case Expr::SIToFP: {
      ref<Expr> lhsKid = lhs->getKid(0), rhsKid = rhs->getKid(0);
      if (lhsKid->getWidth() < rhsKid->getWidth()) {
        lhsKid = (kind == Expr::SIToFP ? SExtExpr::create : ZExtExpr::create)(lhsKid, rhsKid->getWidth());
      } else if (lhsKid->getWidth() > rhsKid->getWidth()) {
        rhsKid = (kind == Expr::SIToFP ? SExtExpr::create : ZExtExpr::create)(rhsKid, lhsKid->getWidth());
      }
      return EqExpr::create(lhsKid, rhsKid);
    }
    default:
      assert(0 && "Floating point value expected");
  }
}

ref<Expr> FPRewritingSolver::rewriteConstraint(const ref<Expr> &e) {
  switch (e->getKind()) {
    case Expr::FOeq:
      return constrainEquality(e->getKid(0), e->getKid(1)); /* I don't think this is right */
    case Expr::FOne:
      return Expr::createIsZero(constrainEquality(e->getKid(0), e->getKid(1)));
    default:
      return e;
  }
}

const ConstraintManager *FPRewritingSolver::rewriteConstraints(const ConstraintManager &cm, bool &changed) {
  std::vector< ref<Expr> > constraints;
  for (ConstraintManager::constraint_iterator i  = cm.begin();
                                              i != cm.end();
                                            ++i) {
    ref<Expr> newConstraint = rewriteConstraint(*i);
    constraints.push_back(newConstraint);
    if (newConstraint != *i)
      changed = true;
  }
  return changed ? new ConstraintManager(constraints) : &cm;
}


bool FPRewritingSolver::computeTruth(const Query &q, bool &isValid) {
  bool changed = false;
  const ConstraintManager *cm = rewriteConstraints(q.constraints, changed);
  bool rv = solver->impl->computeTruth(Query(*cm, rewriteConstraint(q.expr)), isValid);
  if (changed)
    delete cm;
  return rv;
}

bool FPRewritingSolver::computeValidity(const Query &q, Solver::Validity &result) {
  bool changed = false;
  const ConstraintManager *cm = rewriteConstraints(q.constraints, changed);
  bool rv = solver->impl->computeValidity(Query(*cm, rewriteConstraint(q.expr)), result);
  if (changed)
    delete cm;
  return rv;
}

bool FPRewritingSolver::computeValue(const Query &q, ref<Expr> &result) {
  bool changed = false;
  const ConstraintManager *cm = rewriteConstraints(q.constraints, changed);
  bool rv = solver->impl->computeValue(Query(*cm, rewriteConstraint(q.expr)), result);
  if (changed)
    delete cm;
  return rv;
}

bool FPRewritingSolver::computeInitialValues(const Query& query,
                          const std::vector<const Array*> &objects,
                          std::vector< std::vector<unsigned char> > &values,
                          bool &hasSolution) {
  bool changed = false;
  const ConstraintManager *cm = rewriteConstraints(query.constraints, changed);
  bool rv = solver->impl->computeInitialValues(Query(*cm, rewriteConstraint(query.expr)), objects, values,
                                               hasSolution);
  if (changed)
    delete cm;
  return rv;
}

Solver *klee::createFPRewritingSolver(Solver *s) {
  return new Solver(new FPRewritingSolver(s));
}
