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

  ref<Expr> constrainEquality(ref<Expr> lhs, ref<Expr> rhs, bool isUnordered = false);

  ref<Expr> fuseConstraints(const ref<Expr> &e1, const ref<Expr> &e2);

  ref<Expr> rewriteConstraint(const ref<Expr> &e);
  ref<Expr> _rewriteConstraint(const ref<Expr> &e, bool isNeg);

  Query rewriteConstraints(const Query &q);

  bool computeTruth(const Query&, bool &isValid);
  bool computeValidity(const Query&, Solver::Validity &result);
  bool computeValue(const Query&, ref<Expr> &result);
  bool computeInitialValues(const Query& query,
                            const std::vector<const Array*> &objects,
                            std::vector< std::vector<unsigned char> > &values,
                            bool &hasSolution);
};

/* Given a pair of floating point expressions lhs and rhs, return an expression
 * which is a sufficient condition for lhs == rhs (unordered or bitwise
 * comparison) to hold
 *
 * The inverse of the expression is also a necessary condition for
 * lhs != rhs (ordered comparison) to hold
 */
ref<Expr> FPRewritingSolver::constrainEquality(ref<Expr> lhs, ref<Expr> rhs, bool isUnordered) {
  Expr::Kind kind = lhs->getKind();
  if (kind != rhs->getKind())
    return ConstantExpr::alloc(0, Expr::Bool);

  switch (kind) {
    case Expr::FAdd:
    case Expr::FMul:
      return OrExpr::create(
               AndExpr::create(constrainEquality(lhs->getKid(0), rhs->getKid(0), isUnordered),
                               constrainEquality(lhs->getKid(1), rhs->getKid(1), isUnordered)),
               AndExpr::create(constrainEquality(lhs->getKid(0), rhs->getKid(1), isUnordered),
                               constrainEquality(lhs->getKid(1), rhs->getKid(0), isUnordered)));
    case Expr::FSub:
    case Expr::FDiv:
    case Expr::FRem:
      return AndExpr::create(constrainEquality(lhs->getKid(0), rhs->getKid(0), isUnordered),
                             constrainEquality(lhs->getKid(1), rhs->getKid(1), isUnordered));
    case Expr::FCmp: {
      FCmpExpr *clhs = cast<FCmpExpr>(lhs), *crhs = cast<FCmpExpr>(rhs);
      if (clhs->getPredicate() != crhs->getPredicate())
        return ConstantExpr::alloc(0, Expr::Bool);

      if (clhs->isCommutative()) {
        return OrExpr::create(
                 AndExpr::create(constrainEquality(lhs->getKid(0), rhs->getKid(0), isUnordered),
                                 constrainEquality(lhs->getKid(1), rhs->getKid(1), isUnordered)),
                 AndExpr::create(constrainEquality(lhs->getKid(0), rhs->getKid(1), isUnordered),
                                 constrainEquality(lhs->getKid(1), rhs->getKid(0), isUnordered)));
      } else {
        return AndExpr::create(constrainEquality(lhs->getKid(0), rhs->getKid(0), isUnordered),
                               constrainEquality(lhs->getKid(1), rhs->getKid(1), isUnordered));
      }
    }
    case Expr::FPExt:
    case Expr::FPTrunc: {
      F2FConvertExpr *fclhs = cast<F2FConvertExpr>(lhs), *fcrhs = cast<F2FConvertExpr>(rhs);
      if (fclhs->fromIsIEEE() != fcrhs->fromIsIEEE()
       || fclhs->getSemantics() != fcrhs->getSemantics())
        return ConstantExpr::alloc(0, Expr::Bool);
      ref<Expr> lhsKid = lhs->getKid(0), rhsKid = rhs->getKid(0);
      return constrainEquality(lhsKid, rhsKid, isUnordered);
    }
    case Expr::UIToFP:
    case Expr::SIToFP: {
      FConvertExpr *fclhs = cast<FConvertExpr>(lhs), *fcrhs = cast<FConvertExpr>(rhs);
      if (fclhs->getSemantics() != fcrhs->getSemantics())
        return ConstantExpr::alloc(0, Expr::Bool);
      ref<Expr> lhsKid = lhs->getKid(0), rhsKid = rhs->getKid(0);
      if (lhsKid->getWidth() < rhsKid->getWidth()) {
        lhsKid = (kind == Expr::SIToFP ? SExtExpr::create : ZExtExpr::create)(lhsKid, rhsKid->getWidth());
      } else if (lhsKid->getWidth() > rhsKid->getWidth()) {
        rhsKid = (kind == Expr::SIToFP ? SExtExpr::create : ZExtExpr::create)(rhsKid, lhsKid->getWidth());
      }
      return EqExpr::create(lhsKid, rhsKid);
    }
    case Expr::Eq: {
      ref<Expr> lhsNeg, rhsNeg;
      if (lhs->isNotExpr(lhsNeg) && rhs->isNotExpr(rhsNeg))
        return constrainEquality(lhsNeg, rhsNeg, isUnordered);
      else
        return ConstantExpr::alloc(0, Expr::Bool);
    }
    case Expr::Select: {
      if (lhs == rhs)
        return ConstantExpr::alloc(1, Expr::Bool);
      ref<Expr> lhsCond = lhs->getKid(0), rhsCond = rhs->getKid(0);
      if (lhs->getKid(1) == rhs->getKid(2) && lhs->getKid(2) == rhs->getKid(1)) {
        rhsCond = Expr::createIsZero(rhsCond);
        if (lhsCond == rhsCond)
          return ConstantExpr::alloc(1, Expr::Bool);
      }
      return ConstantExpr::alloc(0, Expr::Bool);
    }
    default: break;
      // assert(0 && "Floating point value expected");
  }
  return ConstantExpr::alloc(lhs->compare(*rhs) == 0 ? 1 : 0, Expr::Bool);
}

bool HasFPExpr(ref<Expr> e) {
  if (isa<F2IConvertExpr>(e))
    return false;
  if (isa<FConvertExpr>(e)
   || isa<FOrd1Expr>(e)
   || isa<FBinaryExpr>(e)
   || isa<FCmpExpr>(e)) return true;
  unsigned int numKids = e->getNumKids();
  for (unsigned int k = 0; k < numKids; k++) {
    if (HasFPExpr(e->getKid(k)))
      return true;
  }
  return false;
}

ref<Expr> FPRewritingSolver::_rewriteConstraint(const ref<Expr> &e, bool isNeg) {
  switch (e->getKind()) {
    case Expr::FCmp:
      switch (cast<FCmpExpr>(e)->getPredicate()) {
        case FCmpExpr::UEQ:
          return constrainEquality(e->getKid(0), e->getKid(1), true);
        case FCmpExpr::ONE:
          return Expr::createIsZero(constrainEquality(e->getKid(0), e->getKid(1), true));
        default: break;
      }
      break;
    case Expr::And:
      if (e->getWidth() == 1)
        return AndExpr::create(_rewriteConstraint(e->getKid(0), isNeg), _rewriteConstraint(e->getKid(1), isNeg));
      break;
    case Expr::Eq: {
      ref<Expr> neg;
      if (e->isNotExpr(neg))
        return Expr::createIsZero(_rewriteConstraint(neg, !isNeg));
      if (HasFPExpr(e))
        return constrainEquality(e->getKid(0), e->getKid(1));
      break;
    }
    default: break;
  }
  if (HasFPExpr(e))
    return ConstantExpr::create(isNeg ? 0 : 1, Expr::Bool);
  return e;
}

ref<Expr> FPRewritingSolver::rewriteConstraint(const ref<Expr> &e) {
#ifdef DEBUG_FPRS
  std::cerr << "C+ Input constraint: ";
  e->dump();
#endif
  ref<Expr> ep = _rewriteConstraint(e, false);
#ifdef DEBUG_FPRS
  std::cerr << "C+ Output constraint: ";
  ep->dump();
#endif
  return ep;
}

ref<Expr> FPRewritingSolver::fuseConstraints(const ref<Expr> &e1, const ref<Expr> &e2) {
  return Expr::createIsZero(constrainEquality(Expr::createIsZero(e1), e2));
}

Query FPRewritingSolver::rewriteConstraints(const Query &q) {
  std::vector< ref<Expr> > constraints(q.constraints.begin(), q.constraints.end()), newConstraints;
  constraints.push_back(Expr::createIsZero(q.expr));
  for (std::vector< ref<Expr> >::iterator i  = constraints.begin();
                                          i != constraints.end();
                                        ++i) {
    ref<Expr> oldConstraint = rewriteConstraint(*i), newConstraint = oldConstraint;
    std::vector< ref<Expr> >::iterator j = i; ++j;
    for (;j != constraints.end();
        ++j) {
      newConstraint = AndExpr::create(newConstraint, fuseConstraints(*i, *j));
    }
#ifdef DEBUG_FPRS
    std::cerr << "C+ FINAL constraint: ";
    newConstraint->dump();
#else
    if (oldConstraint != newConstraint) {
      std::cerr << "Fused a constraint!";
      newConstraint->dump();
    }
#endif
    newConstraints.push_back(newConstraint);
  }
  ref<Expr> query = Expr::createIsZero(newConstraints.back());
  newConstraints.pop_back();
  return Query(*new ConstraintManager(newConstraints), query);
}


bool FPRewritingSolver::computeTruth(const Query &q, bool &isValid) {
  Query qp = rewriteConstraints(q);
  bool rv = solver->impl->computeTruth(qp, isValid);
  delete &qp.constraints;
  return rv;
}

bool FPRewritingSolver::computeValidity(const Query &q, Solver::Validity &result) {
  Query qp = rewriteConstraints(q);
  bool rv = solver->impl->computeValidity(qp, result);
  delete &qp.constraints;
  return rv;
}

bool FPRewritingSolver::computeValue(const Query &q, ref<Expr> &result) {
  Query qp = rewriteConstraints(q);
  bool rv = solver->impl->computeValue(qp, result);
  delete &qp.constraints;
  return rv;
}

bool FPRewritingSolver::computeInitialValues(const Query& query,
                          const std::vector<const Array*> &objects,
                          std::vector< std::vector<unsigned char> > &values,
                          bool &hasSolution) {
  Query qp = rewriteConstraints(query);
  bool rv = solver->impl->computeInitialValues(qp, objects, values,
                                               hasSolution);
  delete &qp.constraints;
  return rv;
}

Solver *klee::createFPRewritingSolver(Solver *s) {
  return new Solver(new FPRewritingSolver(s));
}
