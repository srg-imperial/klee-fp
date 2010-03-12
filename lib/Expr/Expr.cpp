//===-- Expr.cpp ----------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Expr.h"

#include "llvm/Support/CommandLine.h"
// FIXME: We shouldn't need this once fast constant support moves into
// Core. If we need to do arithmetic, we probably want to use APInt.
#include "klee/Internal/Support/IntEvaluation.h"

#include "klee/util/ExprPPrinter.h"

#include <iostream>
#include <sstream>

using namespace klee;
using namespace llvm;

namespace {
  cl::opt<bool>
  ConstArrayOpt("const-array-opt",
	 cl::init(false),
	 cl::desc("Enable various optimizations involving all-constant arrays."));
}

/***/

unsigned Expr::count = 0;

ref<Expr> Expr::createTempRead(const Array *array, Expr::Width w) {
  UpdateList ul(array, 0);

  switch (w) {
  default: assert(0 && "invalid width");
  case Expr::Bool: 
    return ZExtExpr::create(ReadExpr::create(ul, 
                                             IConstantExpr::alloc(0, Expr::Int32)),
                            Expr::Bool);
  case Expr::Int8: 
    return ReadExpr::create(ul, 
                            IConstantExpr::alloc(0,Expr::Int32));
  case Expr::Int16: 
    return ConcatExpr::create(ReadExpr::create(ul, 
                                               IConstantExpr::alloc(1,Expr::Int32)),
                              ReadExpr::create(ul, 
                                               IConstantExpr::alloc(0,Expr::Int32)));
  case Expr::Int32: 
    return ConcatExpr::create4(ReadExpr::create(ul, 
                                                IConstantExpr::alloc(3,Expr::Int32)),
                               ReadExpr::create(ul, 
                                                IConstantExpr::alloc(2,Expr::Int32)),
                               ReadExpr::create(ul, 
                                                IConstantExpr::alloc(1,Expr::Int32)),
                               ReadExpr::create(ul, 
                                                IConstantExpr::alloc(0,Expr::Int32)));
  case Expr::Int64: 
    return ConcatExpr::create8(ReadExpr::create(ul, 
                                                IConstantExpr::alloc(7,Expr::Int32)),
                               ReadExpr::create(ul, 
                                                IConstantExpr::alloc(6,Expr::Int32)),
                               ReadExpr::create(ul, 
                                                IConstantExpr::alloc(5,Expr::Int32)),
                               ReadExpr::create(ul, 
                                                IConstantExpr::alloc(4,Expr::Int32)),
                               ReadExpr::create(ul, 
                                                IConstantExpr::alloc(3,Expr::Int32)),
                               ReadExpr::create(ul, 
                                                IConstantExpr::alloc(2,Expr::Int32)),
                               ReadExpr::create(ul, 
                                                IConstantExpr::alloc(1,Expr::Int32)),
                               ReadExpr::create(ul, 
                                                IConstantExpr::alloc(0,Expr::Int32)));
  }
}

// returns 0 if b is structurally equal to *this
int Expr::compare(const Expr &b) const {
  if (this == &b) return 0;

  Kind ak = getKind(), bk = b.getKind();
  if (ak!=bk)
    return (ak < bk) ? -1 : 1;

  if (hashValue != b.hashValue) 
    return (hashValue < b.hashValue) ? -1 : 1;

  if (int res = compareContents(b)) 
    return res;

  unsigned aN = getNumKids();
  for (unsigned i=0; i<aN; i++)
    if (int res = getKid(i).compare(b.getKid(i)))
      return res;

  return 0;
}

void Expr::printKind(std::ostream &os, Kind k) {
  switch(k) {
#define X(C) case C: os << #C; break
    X(IConstant);
    X(FConstant);
    X(NotOptimized);
    X(Read);
    X(Select);
    X(Concat);
    X(Extract);
    X(ZExt);
    X(SExt);
    X(FPExt);
    X(FPTrunc);
    X(UIToFP);
    X(SIToFP);
    X(FOrd1);
    X(Add);
    X(Sub);
    X(Mul);
    X(UDiv);
    X(SDiv);
    X(URem);
    X(SRem);
    X(FAdd);
    X(FSub);
    X(FMul);
    X(FDiv);
    X(FRem);
    X(Not);
    X(And);
    X(Or);
    X(Xor);
    X(Shl);
    X(LShr);
    X(AShr);
    X(Eq);
    X(Ne);
    X(Ult);
    X(Ule);
    X(Ugt);
    X(Uge);
    X(Slt);
    X(Sle);
    X(Sgt);
    X(Sge);
    X(FOrd);
    X(FOeq);
    X(FOlt);
    X(FOle);
    X(FOgt);
    X(FOge);
    X(FOne);
    X(FUno);
    X(FUeq);
    X(FUlt);
    X(FUle);
    X(FUgt);
    X(FUge);
    X(FUne);
#undef X
  default:
    assert(0 && "invalid kind");
    }
}

////////
//
// Simple hash functions for various kinds of Exprs
//
///////

unsigned Expr::computeHash() {
  unsigned res = getKind() * Expr::MAGIC_HASH_CONSTANT;

  int n = getNumKids();
  for (int i = 0; i < n; i++) {
    res <<= 1;
    res ^= getKid(i)->hash() * Expr::MAGIC_HASH_CONSTANT;
  }
  
  hashValue = res;
  return hashValue;
}

unsigned IConstantExpr::computeHash() {
  hashValue = value.getHashValue() ^ (getWidth() * MAGIC_HASH_CONSTANT);
  return hashValue;
}

unsigned FConstantExpr::computeHash() {
  hashValue = value.getHashValue() ^ (getWidth() * MAGIC_HASH_CONSTANT);
  return hashValue;
}

unsigned CastExpr::computeHash() {
  unsigned res = getWidth() * Expr::MAGIC_HASH_CONSTANT;
  hashValue = res ^ src->hash() * Expr::MAGIC_HASH_CONSTANT;
  return hashValue;
}

unsigned FConvertExpr::computeHash() {
  unsigned res = getWidth() * Expr::MAGIC_HASH_CONSTANT;
  hashValue = res ^ src->hash() * Expr::MAGIC_HASH_CONSTANT;
  return hashValue;
}

unsigned ExtractExpr::computeHash() {
  unsigned res = offset * Expr::MAGIC_HASH_CONSTANT;
  res ^= getWidth() * Expr::MAGIC_HASH_CONSTANT;
  hashValue = res ^ expr->hash() * Expr::MAGIC_HASH_CONSTANT;
  return hashValue;
}

unsigned ReadExpr::computeHash() {
  unsigned res = index->hash() * Expr::MAGIC_HASH_CONSTANT;
  res ^= updates.hash();
  hashValue = res;
  return hashValue;
}

unsigned NotExpr::computeHash() {
  unsigned hashValue = expr->hash() * Expr::MAGIC_HASH_CONSTANT * Expr::Not;
  return hashValue;
}

ref<Expr> Expr::createFromKind(Kind k, std::vector<CreateArg> args) {
  unsigned numArgs = args.size();
  (void) numArgs;

  switch(k) {
    case IConstant:
    case Extract:
    case Read:
    default:
      assert(0 && "invalid kind");

    case NotOptimized:
      assert(numArgs == 1 && args[0].isExpr() &&
             "invalid args array for given opcode");
      return NotOptimizedExpr::create(args[0].expr);
      
    case Select:
      assert(numArgs == 3 && args[0].isExpr() &&
             args[1].isExpr() && args[2].isExpr() &&
             "invalid args array for Select opcode");
      return SelectExpr::create(args[0].expr,
                                args[1].expr,
                                args[2].expr);

    case Concat: {
      assert(numArgs == 2 && args[0].isExpr() && args[1].isExpr() && 
             "invalid args array for Concat opcode");
      
      return ConcatExpr::create(args[0].expr, args[1].expr);
    }
      
#define CAST_EXPR_CASE(T)                                    \
      case T:                                                \
        assert(numArgs == 2 &&				     \
               args[0].isExpr() && args[1].isWidth() &&      \
               "invalid args array for given opcode");       \
      return T ## Expr::create(args[0].expr, args[1].width); \
      
#define BINARY_EXPR_CASE(T)                                 \
      case T:                                               \
        assert(numArgs == 2 &&                              \
               args[0].isExpr() && args[1].isExpr() &&      \
               "invalid args array for given opcode");      \
      return T ## Expr::create(args[0].expr, args[1].expr); \

      CAST_EXPR_CASE(ZExt);
      CAST_EXPR_CASE(SExt);
      
      BINARY_EXPR_CASE(Add);
      BINARY_EXPR_CASE(Sub);
      BINARY_EXPR_CASE(Mul);
      BINARY_EXPR_CASE(UDiv);
      BINARY_EXPR_CASE(SDiv);
      BINARY_EXPR_CASE(URem);
      BINARY_EXPR_CASE(SRem);
      BINARY_EXPR_CASE(And);
      BINARY_EXPR_CASE(Or);
      BINARY_EXPR_CASE(Xor);
      BINARY_EXPR_CASE(Shl);
      BINARY_EXPR_CASE(LShr);
      BINARY_EXPR_CASE(AShr);
      
      BINARY_EXPR_CASE(Eq);
      BINARY_EXPR_CASE(Ne);
      BINARY_EXPR_CASE(Ult);
      BINARY_EXPR_CASE(Ule);
      BINARY_EXPR_CASE(Ugt);
      BINARY_EXPR_CASE(Uge);
      BINARY_EXPR_CASE(Slt);
      BINARY_EXPR_CASE(Sle);
      BINARY_EXPR_CASE(Sgt);
      BINARY_EXPR_CASE(Sge);
  }
}


void Expr::printWidth(std::ostream &os, Width width) {
  switch(width) {
  case Expr::Bool: os << "Expr::Bool"; break;
  case Expr::Int8: os << "Expr::Int8"; break;
  case Expr::Int16: os << "Expr::Int16"; break;
  case Expr::Int32: os << "Expr::Int32"; break;
  case Expr::Int64: os << "Expr::Int64"; break;
  default: os << "<invalid type: " << (unsigned) width << ">";
  }
}

ref<Expr> Expr::createImplies(ref<Expr> hyp, ref<Expr> conc) {
  return OrExpr::create(Expr::createIsZero(hyp), conc);
}

ref<Expr> Expr::createIsZero(ref<Expr> e) {
  return EqExpr::create(e, IConstantExpr::create(0, e->getWidth()));
}

void Expr::print(std::ostream &os) const {
  ExprPPrinter::printSingleExpr(os, (Expr*)this);
}

void Expr::dump() const {
  this->print(std::cerr);
  std::cerr << std::endl;
}

/***/

ref<Expr> IConstantExpr::fromMemory(void *address, Width width) {
  switch (width) {
  default: assert(0 && "invalid type");
  case  Expr::Bool: return IConstantExpr::create(*(( uint8_t*) address), width);
  case  Expr::Int8: return IConstantExpr::create(*(( uint8_t*) address), width);
  case Expr::Int16: return IConstantExpr::create(*((uint16_t*) address), width);
  case Expr::Int32: return IConstantExpr::create(*((uint32_t*) address), width);
  case Expr::Int64: return IConstantExpr::create(*((uint64_t*) address), width);
    // FIXME: Should support long double, at least.
  }
}

void IConstantExpr::toMemory(void *address) {
  switch (getWidth()) {
  default: assert(0 && "invalid type");
  case  Expr::Bool: *(( uint8_t*) address) = getZExtValue(1); break;
  case  Expr::Int8: *(( uint8_t*) address) = getZExtValue(8); break;
  case Expr::Int16: *((uint16_t*) address) = getZExtValue(16); break;
  case Expr::Int32: *((uint32_t*) address) = getZExtValue(32); break;
  case Expr::Int64: *((uint64_t*) address) = getZExtValue(64); break;
    // FIXME: Should support long double, at least.
  }
}

void IConstantExpr::toString(std::string &Res) const {
  Res = value.toString(10, false);
}

ref<IConstantExpr> IConstantExpr::Concat(const ref<IConstantExpr> &RHS) {
  Expr::Width W = getWidth() + RHS->getWidth();
  APInt Tmp(value);
  Tmp.zext(W);
  Tmp <<= RHS->getWidth();
  Tmp |= APInt(RHS->value).zext(W);

  return IConstantExpr::alloc(Tmp);
}

ref<IConstantExpr> IConstantExpr::Extract(unsigned Offset, Width W) {
  return IConstantExpr::alloc(APInt(value.ashr(Offset)).zextOrTrunc(W));
}

ref<IConstantExpr> IConstantExpr::ZExt(Width W) {
  return IConstantExpr::alloc(APInt(value).zextOrTrunc(W));
}

ref<IConstantExpr> IConstantExpr::SExt(Width W) {
  return IConstantExpr::alloc(APInt(value).sextOrTrunc(W));
}

ref<IConstantExpr> IConstantExpr::Add(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value + RHS->value);
}

ref<IConstantExpr> IConstantExpr::Neg() {
  return IConstantExpr::alloc(-value);
}

ref<IConstantExpr> IConstantExpr::Sub(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value - RHS->value);
}

ref<IConstantExpr> IConstantExpr::Mul(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value * RHS->value);
}

ref<IConstantExpr> IConstantExpr::UDiv(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.udiv(RHS->value));
}

ref<IConstantExpr> IConstantExpr::SDiv(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.sdiv(RHS->value));
}

ref<IConstantExpr> IConstantExpr::URem(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.urem(RHS->value));
}

ref<IConstantExpr> IConstantExpr::SRem(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.srem(RHS->value));
}

ref<IConstantExpr> IConstantExpr::And(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value & RHS->value);
}

ref<IConstantExpr> IConstantExpr::Or(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value | RHS->value);
}

ref<IConstantExpr> IConstantExpr::Xor(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value ^ RHS->value);
}

ref<IConstantExpr> IConstantExpr::Shl(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.shl(RHS->value));
}

ref<IConstantExpr> IConstantExpr::LShr(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.lshr(RHS->value));
}

ref<IConstantExpr> IConstantExpr::AShr(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.ashr(RHS->value));
}

ref<IConstantExpr> IConstantExpr::Not() {
  return IConstantExpr::alloc(~value);
}

ref<IConstantExpr> IConstantExpr::Eq(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value == RHS->value, Expr::Bool);
}

ref<IConstantExpr> IConstantExpr::Ne(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value != RHS->value, Expr::Bool);
}

ref<IConstantExpr> IConstantExpr::Ult(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.ult(RHS->value), Expr::Bool);
}

ref<IConstantExpr> IConstantExpr::Ule(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.ule(RHS->value), Expr::Bool);
}

ref<IConstantExpr> IConstantExpr::Ugt(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.ugt(RHS->value), Expr::Bool);
}

ref<IConstantExpr> IConstantExpr::Uge(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.uge(RHS->value), Expr::Bool);
}

ref<IConstantExpr> IConstantExpr::Slt(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.slt(RHS->value), Expr::Bool);
}

ref<IConstantExpr> IConstantExpr::Sle(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.sle(RHS->value), Expr::Bool);
}

ref<IConstantExpr> IConstantExpr::Sgt(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.sgt(RHS->value), Expr::Bool);
}

ref<IConstantExpr> IConstantExpr::Sge(const ref<IConstantExpr> &RHS) {
  return IConstantExpr::alloc(value.sge(RHS->value), Expr::Bool);
}

ref<FConstantExpr> IConstantExpr::UIToFP(const fltSemantics *sem) {
  return IToFP(sem, false);
}

ref<FConstantExpr> IConstantExpr::SIToFP(const fltSemantics *sem) {
  return IToFP(sem, true);
}

ref<FConstantExpr> IConstantExpr::IToFP(const fltSemantics *sem, bool isUnsigned) {
  APFloat res(*sem, 0);
  res.convertFromAPInt(value,
                       isUnsigned,
                       APFloat::rmTowardZero);
  return FConstantExpr::create(res);
}

/***/

unsigned FPExpr::getWidth() const {
  const fltSemantics *s = getSemantics();
  if (s == &APFloat::IEEEsingle) 
    return 32;
  if (s == &APFloat::IEEEdouble) 
    return 64;
  if (s == &APFloat::IEEEquad || s == &APFloat::PPCDoubleDouble) 
    return 128;
  if (s == &APFloat::x87DoubleExtended) 
    return 80;

  assert(0 && "unknown format");
  return 0;
}

void FConstantExpr::toMemory(void *address) {
  const fltSemantics *s = &value.getSemantics();
       if (s == &APFloat::IEEEsingle) 
    *((float  *) address) = value.convertToFloat();
  else if (s == &APFloat::IEEEdouble) 
    *((double *) address) = value.convertToDouble();
  else
    assert(0 && "unknown format");
}

FPExpr::FPCategories FConstantExpr::getCategories() const {
  if (value.isZero())
    return fcMaybeZero;
  if (value.isInfinity()) {
    if (value.isNegative())
      return fcMaybeNInf;
    else
      return fcMaybePInf;
  }
  if (value.isNaN())
    return fcMaybeNaN;
  return value.isNegative() ? fcMaybeNNorm : fcMaybePNorm;
}

ref<FConstantExpr> FConstantExpr::FAdd(const ref<FConstantExpr> &RHS) {
  APFloat f = value;
  f.add(RHS->value, APFloat::rmNearestTiesToEven);
  return FConstantExpr::create(f);
}

ref<FConstantExpr> FConstantExpr::FSub(const ref<FConstantExpr> &RHS) {
  APFloat f = value;
  f.subtract(RHS->value, APFloat::rmNearestTiesToEven);
  return FConstantExpr::create(f);
}

ref<FConstantExpr> FConstantExpr::FMul(const ref<FConstantExpr> &RHS) {
  APFloat f = value;
  f.multiply(RHS->value, APFloat::rmNearestTiesToEven);
  return FConstantExpr::create(f);
}

ref<FConstantExpr> FConstantExpr::FDiv(const ref<FConstantExpr> &RHS) {
  APFloat f = value;
  f.divide(RHS->value, APFloat::rmNearestTiesToEven);
  return FConstantExpr::create(f);
}

ref<FConstantExpr> FConstantExpr::FRem(const ref<FConstantExpr> &RHS) {
  APFloat f = value;
  f.mod(RHS->value, APFloat::rmNearestTiesToEven);
  return FConstantExpr::create(f);
}

ref<FConstantExpr> FConstantExpr::FPExt(const fltSemantics *sem) {
  APFloat f = value;
  bool losesInfo;
  f.convert(*sem, APFloat::rmNearestTiesToEven, &losesInfo);
  return FConstantExpr::create(f);
}

ref<FConstantExpr> FConstantExpr::FPTrunc(const fltSemantics *sem) {
  return FPExt(sem);
}

ref<IConstantExpr> FConstantExpr::FOeq(const ref<FConstantExpr> &RHS) {
  return IConstantExpr::create(value.compare(RHS->value) == APFloat::cmpEqual, Expr::Bool);
}

ref<IConstantExpr> FConstantExpr::FOlt(const ref<FConstantExpr> &RHS) {
  return IConstantExpr::create(value.compare(RHS->value) == APFloat::cmpLessThan, Expr::Bool);
}

ref<IConstantExpr> FConstantExpr::FOrd() {
  return IConstantExpr::create(!value.isNaN(), Expr::Bool);
}

int FConstantExpr::compareContents(const Expr &b) const {
  const FConstantExpr &cb = static_cast<const FConstantExpr&>(b);
  const fltSemantics *s1 = getSemantics(),
                     *s2 = cb.getSemantics();
  if (s1 != s2)
    return s1 < s2 ? -1 : 1;

  /* We order FP values using their bitwise representations */
  APInt i1 = value.bitcastToAPInt(),
        i2 = cb.value.bitcastToAPInt();
  assert(i1.getBitWidth() == i2.getBitWidth() && "bitwidths of FP constants with same semantics unequal");
  if (i1 == i2)
    return 0;
  return i1.ult(i2) ? -1 : 1;
}

/***/

ref<Expr>  NotOptimizedExpr::create(ref<Expr> src) {
  return NotOptimizedExpr::alloc(src);
}

/***/

extern "C" void vc_DeleteExpr(void*);

Array::~Array() {
  // FIXME: This shouldn't be necessary.
  if (stpInitialArray) {
    ::vc_DeleteExpr(stpInitialArray);
    stpInitialArray = 0;
  }
}

/***/

ref<Expr> ReadExpr::create(const UpdateList &ul, ref<Expr> index) {
  // rollback index when possible... 

  // XXX this doesn't really belong here... there are basically two
  // cases, one is rebuild, where we want to optimistically try various
  // optimizations when the index has changed, and the other is 
  // initial creation, where we expect the ObjectState to have constructed
  // a smart UpdateList so it is not worth rescanning.

  const UpdateNode *un = ul.head;
  for (; un; un=un->next) {
    ref<Expr> cond = EqExpr::create(index, un->index);
    
    if (IConstantExpr *CE = dyn_cast<IConstantExpr>(cond)) {
      if (CE->isTrue())
        return un->value;
    } else {
      break;
    }
  }

  return ReadExpr::alloc(ul, index);
}

int ReadExpr::compareContents(const Expr &b) const { 
  return updates.compare(static_cast<const ReadExpr&>(b).updates);
}

ref<Expr> SelectExpr::create(ref<Expr> c, ref<Expr> t, ref<Expr> f) {
  Expr::Width kt = t->getWidth();

  assert(c->getWidth()==Bool && "type mismatch");
  assert(kt==f->getWidth() && "type mismatch");

  if (IConstantExpr *CE = dyn_cast<IConstantExpr>(c)) {
    return CE->isTrue() ? t : f;
  } else if (t==f) {
    return t;
  } else if (kt==Expr::Bool) { // c ? t : f  <=> (c and t) or (not c and f)
    if (IConstantExpr *CE = dyn_cast<IConstantExpr>(t)) {      
      if (CE->isTrue()) {
        return OrExpr::create(c, f);
      } else {
        return AndExpr::create(Expr::createIsZero(c), f);
      }
    } else if (IConstantExpr *CE = dyn_cast<IConstantExpr>(f)) {
      if (CE->isTrue()) {
        return OrExpr::create(Expr::createIsZero(c), t);
      } else {
        return AndExpr::create(c, t);
      }
    }
  }
  
  return SelectExpr::alloc(c, t, f);
}

/***/

ref<Expr> ConcatExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  Expr::Width w = l->getWidth() + r->getWidth();
  
  // Fold concatenation of constants.
  //
  // FIXME: concat 0 x -> zext x ?
  if (IConstantExpr *lCE = dyn_cast<IConstantExpr>(l))
    if (IConstantExpr *rCE = dyn_cast<IConstantExpr>(r))
      return lCE->Concat(rCE);

  // Merge contiguous Extracts
  if (ExtractExpr *ee_left = dyn_cast<ExtractExpr>(l)) {
    if (ExtractExpr *ee_right = dyn_cast<ExtractExpr>(r)) {
      if (ee_left->expr == ee_right->expr &&
          ee_right->offset + ee_right->width == ee_left->offset) {
        return ExtractExpr::create(ee_left->expr, ee_right->offset, w);
      }
    }
  }

  return ConcatExpr::alloc(l, r);
}

/// Shortcut to concat N kids.  The chain returned is unbalanced to the right
ref<Expr> ConcatExpr::createN(unsigned n_kids, const ref<Expr> kids[]) {
  assert(n_kids > 0);
  if (n_kids == 1)
    return kids[0];
  
  ref<Expr> r = ConcatExpr::create(kids[n_kids-2], kids[n_kids-1]);
  for (int i=n_kids-3; i>=0; i--)
    r = ConcatExpr::create(kids[i], r);
  return r;
}

/// Shortcut to concat 4 kids.  The chain returned is unbalanced to the right
ref<Expr> ConcatExpr::create4(const ref<Expr> &kid1, const ref<Expr> &kid2,
                              const ref<Expr> &kid3, const ref<Expr> &kid4) {
  return ConcatExpr::create(kid1, ConcatExpr::create(kid2, ConcatExpr::create(kid3, kid4)));
}

/// Shortcut to concat 8 kids.  The chain returned is unbalanced to the right
ref<Expr> ConcatExpr::create8(const ref<Expr> &kid1, const ref<Expr> &kid2,
			      const ref<Expr> &kid3, const ref<Expr> &kid4,
			      const ref<Expr> &kid5, const ref<Expr> &kid6,
			      const ref<Expr> &kid7, const ref<Expr> &kid8) {
  return ConcatExpr::create(kid1, ConcatExpr::create(kid2, ConcatExpr::create(kid3, 
			      ConcatExpr::create(kid4, ConcatExpr::create4(kid5, kid6, kid7, kid8)))));
}

/***/

ref<Expr> ExtractExpr::create(ref<Expr> expr, unsigned off, Width w) {
  unsigned kw = expr->getWidth();
  assert(w > 0 && off + w <= kw && "invalid extract");
  
  if (w == kw) {
    return expr;
  } else if (IConstantExpr *CE = dyn_cast<IConstantExpr>(expr)) {
    return CE->Extract(off, w);
  } else {
    // Extract(Concat)
    if (ConcatExpr *ce = dyn_cast<ConcatExpr>(expr)) {
      // if the extract skips the right side of the concat
      if (off >= ce->getRight()->getWidth())
	return ExtractExpr::create(ce->getLeft(), off - ce->getRight()->getWidth(), w);
      
      // if the extract skips the left side of the concat
      if (off + w <= ce->getRight()->getWidth())
	return ExtractExpr::create(ce->getRight(), off, w);

      // E(C(x,y)) = C(E(x), E(y))
      return ConcatExpr::create(ExtractExpr::create(ce->getKid(0), 0, w - ce->getKid(1)->getWidth() + off),
				ExtractExpr::create(ce->getKid(1), off, ce->getKid(1)->getWidth() - off));
    }
  }
  
  return ExtractExpr::alloc(expr, off, w);
}

/***/

ref<Expr> NotExpr::create(const ref<Expr> &e) {
  if (IConstantExpr *CE = dyn_cast<IConstantExpr>(e))
    return CE->Not();
  
  return NotExpr::alloc(e);
}


/***/

ref<Expr> ZExtExpr::create(const ref<Expr> &e, Width w) {
  unsigned kBits = e->getWidth();
  if (w == kBits) {
    return e;
  } else if (w < kBits) { // trunc
    return ExtractExpr::create(e, 0, w);
  } else if (IConstantExpr *CE = dyn_cast<IConstantExpr>(e)) {
    return CE->ZExt(w);
  } else {
    return ZExtExpr::alloc(e, w);
  }
}

ref<Expr> SExtExpr::create(const ref<Expr> &e, Width w) {
  unsigned kBits = e->getWidth();
  if (w == kBits) {
    return e;
  } else if (w < kBits) { // trunc
    return ExtractExpr::create(e, 0, w);
  } else if (IConstantExpr *CE = dyn_cast<IConstantExpr>(e)) {
    return CE->SExt(w);
  } else {    
    return SExtExpr::alloc(e, w);
  }
}

/***/

#define FCCREATE(_e_op, _op) \
ref<Expr> _e_op::create(const ref<Expr> &e, const fltSemantics *sem) { \
  FPExpr *fe = e->asFPExpr();                                          \
  assert(fe && "arg must be an FPExpr");                               \
  if (fe->getSemantics() == sem) {                                     \
    return e;                                                          \
  } else if (FConstantExpr *CE = dyn_cast<FConstantExpr>(e)) {         \
    return CE->_op(sem);                                               \
  } else {                                                             \
    return _e_op::alloc(e, sem);                                       \
  }                                                                    \
}

FCCREATE(FPExtExpr, FPExt)
FCCREATE(FPTruncExpr, FPTrunc)

#define I2FCREATE(_e_op, _op) \
ref<Expr> _e_op::create(const ref<Expr> &e, const fltSemantics *sem) { \
  if (IConstantExpr *CE = dyn_cast<IConstantExpr>(e)) {                \
    return CE->_op(sem);                                               \
  } else {                                                             \
    return _e_op::alloc(e, sem);                                       \
  }                                                                    \
}

I2FCREATE(UIToFPExpr, UIToFP)
I2FCREATE(SIToFPExpr, SIToFP)

/***/

static ref<Expr> AndExpr_create(Expr *l, Expr *r);
static ref<Expr> XorExpr_create(Expr *l, Expr *r);

static ref<Expr> EqExpr_createPartial(Expr *l, const ref<IConstantExpr> &cr);
static ref<Expr> AndExpr_createPartialR(const ref<IConstantExpr> &cl, Expr *r);
static ref<Expr> SubExpr_createPartialR(const ref<IConstantExpr> &cl, Expr *r);
static ref<Expr> XorExpr_createPartialR(const ref<IConstantExpr> &cl, Expr *r);

static ref<Expr> AddExpr_createPartialR(const ref<IConstantExpr> &cl, Expr *r) {
  Expr::Width type = cl->getWidth();

  if (type==Expr::Bool) {
    return XorExpr_createPartialR(cl, r);
  } else if (cl->isZero()) {
    return r;
  } else {
    Expr::Kind rk = r->getKind();
    if (rk==Expr::Add && isa<IConstantExpr>(r->getKid(0))) { // A + (B+c) == (A+B) + c
      return AddExpr::create(AddExpr::create(cl, r->getKid(0)),
                             r->getKid(1));
    } else if (rk==Expr::Sub && isa<IConstantExpr>(r->getKid(0))) { // A + (B-c) == (A+B) - c
      return SubExpr::create(AddExpr::create(cl, r->getKid(0)),
                             r->getKid(1));
    } else {
      return AddExpr::alloc(cl, r);
    }
  }
}
static ref<Expr> AddExpr_createPartial(Expr *l, const ref<IConstantExpr> &cr) {
  return AddExpr_createPartialR(cr, l);
}
static ref<Expr> AddExpr_create(Expr *l, Expr *r) {
  Expr::Width type = l->getWidth();

  if (type == Expr::Bool) {
    return XorExpr_create(l, r);
  } else {
    Expr::Kind lk = l->getKind(), rk = r->getKind();
    if (lk==Expr::Add && isa<IConstantExpr>(l->getKid(0))) { // (k+a)+b = k+(a+b)
      return AddExpr::create(l->getKid(0),
                             AddExpr::create(l->getKid(1), r));
    } else if (lk==Expr::Sub && isa<IConstantExpr>(l->getKid(0))) { // (k-a)+b = k+(b-a)
      return AddExpr::create(l->getKid(0),
                             SubExpr::create(r, l->getKid(1)));
    } else if (rk==Expr::Add && isa<IConstantExpr>(r->getKid(0))) { // a + (k+b) = k+(a+b)
      return AddExpr::create(r->getKid(0),
                             AddExpr::create(l, r->getKid(1)));
    } else if (rk==Expr::Sub && isa<IConstantExpr>(r->getKid(0))) { // a + (k-b) = k+(a-b)
      return AddExpr::create(r->getKid(0),
                             SubExpr::create(l, r->getKid(1)));
    } else {
      return AddExpr::alloc(l, r);
    }
  }  
}

static ref<Expr> SubExpr_createPartialR(const ref<IConstantExpr> &cl, Expr *r) {
  Expr::Width type = cl->getWidth();

  if (type==Expr::Bool) {
    return XorExpr_createPartialR(cl, r);
  } else {
    Expr::Kind rk = r->getKind();
    if (rk==Expr::Add && isa<IConstantExpr>(r->getKid(0))) { // A - (B+c) == (A-B) - c
      return SubExpr::create(SubExpr::create(cl, r->getKid(0)),
                             r->getKid(1));
    } else if (rk==Expr::Sub && isa<IConstantExpr>(r->getKid(0))) { // A - (B-c) == (A-B) + c
      return AddExpr::create(SubExpr::create(cl, r->getKid(0)),
                             r->getKid(1));
    } else {
      return SubExpr::alloc(cl, r);
    }
  }
}
static ref<Expr> SubExpr_createPartial(Expr *l, const ref<IConstantExpr> &cr) {
  // l - c => l + (-c)
  return AddExpr_createPartial(l, 
                               IConstantExpr::alloc(0, cr->getWidth())->Sub(cr));
}
static ref<Expr> SubExpr_create(Expr *l, Expr *r) {
  Expr::Width type = l->getWidth();

  if (type == Expr::Bool) {
    return XorExpr_create(l, r);
  } else if (*l==*r) {
    return IConstantExpr::alloc(0, type);
  } else {
    Expr::Kind lk = l->getKind(), rk = r->getKind();
    if (lk==Expr::Add && isa<IConstantExpr>(l->getKid(0))) { // (k+a)-b = k+(a-b)
      return AddExpr::create(l->getKid(0),
                             SubExpr::create(l->getKid(1), r));
    } else if (lk==Expr::Sub && isa<IConstantExpr>(l->getKid(0))) { // (k-a)-b = k-(a+b)
      return SubExpr::create(l->getKid(0),
                             AddExpr::create(l->getKid(1), r));
    } else if (rk==Expr::Add && isa<IConstantExpr>(r->getKid(0))) { // a - (k+b) = (a-c) - k
      return SubExpr::create(SubExpr::create(l, r->getKid(1)),
                             r->getKid(0));
    } else if (rk==Expr::Sub && isa<IConstantExpr>(r->getKid(0))) { // a - (k-b) = (a+b) - k
      return SubExpr::create(AddExpr::create(l, r->getKid(1)),
                             r->getKid(0));
    } else {
      return SubExpr::alloc(l, r);
    }
  }  
}

static ref<Expr> MulExpr_createPartialR(const ref<IConstantExpr> &cl, Expr *r) {
  Expr::Width type = cl->getWidth();

  if (type == Expr::Bool) {
    return AndExpr_createPartialR(cl, r);
  } else if (cl->isOne()) {
    return r;
  } else if (cl->isZero()) {
    return cl;
  } else {
    return MulExpr::alloc(cl, r);
  }
}
static ref<Expr> MulExpr_createPartial(Expr *l, const ref<IConstantExpr> &cr) {
  return MulExpr_createPartialR(cr, l);
}
static ref<Expr> MulExpr_create(Expr *l, Expr *r) {
  Expr::Width type = l->getWidth();
  
  if (type == Expr::Bool) {
    return AndExpr::alloc(l, r);
  } else {
    return MulExpr::alloc(l, r);
  }
}

static ref<Expr> AndExpr_createPartial(Expr *l, const ref<IConstantExpr> &cr) {
  if (cr->isAllOnes()) {
    return l;
  } else if (cr->isZero()) {
    return cr;
  } else {
    return AndExpr::alloc(l, cr);
  }
}
static ref<Expr> AndExpr_createPartialR(const ref<IConstantExpr> &cl, Expr *r) {
  return AndExpr_createPartial(r, cl);
}
static ref<Expr> AndExpr_create(Expr *l, Expr *r) {
  return AndExpr::alloc(l, r);
}

static ref<Expr> OrExpr_createPartial(Expr *l, const ref<IConstantExpr> &cr) {
  if (cr->isAllOnes()) {
    return cr;
  } else if (cr->isZero()) {
    return l;
  } else {
    return OrExpr::alloc(l, cr);
  }
}
static ref<Expr> OrExpr_createPartialR(const ref<IConstantExpr> &cl, Expr *r) {
  return OrExpr_createPartial(r, cl);
}
static ref<Expr> OrExpr_create(Expr *l, Expr *r) {
  return OrExpr::alloc(l, r);
}

static ref<Expr> XorExpr_createPartialR(const ref<IConstantExpr> &cl, Expr *r) {
  if (cl->isZero()) {
    return r;
  } else if (cl->getWidth() == Expr::Bool) {
    return EqExpr_createPartial(r, IConstantExpr::create(0, Expr::Bool));
  } else {
    return XorExpr::alloc(cl, r);
  }
}

static ref<Expr> XorExpr_createPartial(Expr *l, const ref<IConstantExpr> &cr) {
  return XorExpr_createPartialR(cr, l);
}
static ref<Expr> XorExpr_create(Expr *l, Expr *r) {
  return XorExpr::alloc(l, r);
}

static ref<Expr> UDivExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // r must be 1
    return l;
  } else{
    return UDivExpr::alloc(l, r);
  }
}

static ref<Expr> SDivExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // r must be 1
    return l;
  } else{
    return SDivExpr::alloc(l, r);
  }
}

static ref<Expr> URemExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // r must be 1
    return IConstantExpr::create(0, Expr::Bool);
  } else{
    return URemExpr::alloc(l, r);
  }
}

static ref<Expr> SRemExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // r must be 1
    return IConstantExpr::create(0, Expr::Bool);
  } else{
    return SRemExpr::alloc(l, r);
  }
}

static ref<Expr> ShlExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // l & !r
    return AndExpr::create(l, Expr::createIsZero(r));
  } else{
    return ShlExpr::alloc(l, r);
  }
}

static ref<Expr> LShrExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // l & !r
    return AndExpr::create(l, Expr::createIsZero(r));
  } else{
    return LShrExpr::alloc(l, r);
  }
}

static ref<Expr> AShrExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // l
    return l;
  } else{
    return AShrExpr::alloc(l, r);
  }
}

#define BCREATE_R(_e_op, _op, partialL, partialR) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r) { \
  assert(l->getWidth()==r->getWidth() && "type mismatch");              \
  if (IConstantExpr *cl = dyn_cast<IConstantExpr>(l)) {                   \
    if (IConstantExpr *cr = dyn_cast<IConstantExpr>(r))                   \
      return cl->_op(cr);                                               \
    return _e_op ## _createPartialR(cl, r.get());                       \
  } else if (IConstantExpr *cr = dyn_cast<IConstantExpr>(r)) {            \
    return _e_op ## _createPartial(l.get(), cr);                        \
  }                                                                     \
  return _e_op ## _create(l.get(), r.get());                            \
}

#define BCREATE(_e_op, _op) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r) { \
  assert(l->getWidth()==r->getWidth() && "type mismatch");          \
  if (IConstantExpr *cl = dyn_cast<IConstantExpr>(l))                 \
    if (IConstantExpr *cr = dyn_cast<IConstantExpr>(r))               \
      return cl->_op(cr);                                           \
  return _e_op ## _create(l, r);                                    \
}

BCREATE_R(AddExpr, Add, AddExpr_createPartial, AddExpr_createPartialR)
BCREATE_R(SubExpr, Sub, SubExpr_createPartial, SubExpr_createPartialR)
BCREATE_R(MulExpr, Mul, MulExpr_createPartial, MulExpr_createPartialR)
BCREATE_R(AndExpr, And, AndExpr_createPartial, AndExpr_createPartialR)
BCREATE_R(OrExpr, Or, OrExpr_createPartial, OrExpr_createPartialR)
BCREATE_R(XorExpr, Xor, XorExpr_createPartial, XorExpr_createPartialR)
BCREATE(UDivExpr, UDiv)
BCREATE(SDivExpr, SDiv)
BCREATE(URemExpr, URem)
BCREATE(SRemExpr, SRem)
BCREATE(ShlExpr, Shl)
BCREATE(LShrExpr, LShr)
BCREATE(AShrExpr, AShr)

static ref<Expr> FAddExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  return FAddExpr::alloc(l, r);
}

static ref<Expr> FSubExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  return FSubExpr::alloc(l, r);
}

static ref<Expr> FMulExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  return FMulExpr::alloc(l, r);
}

static ref<Expr> FDivExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  return FDivExpr::alloc(l, r);
}

static ref<Expr> FRemExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  return FRemExpr::alloc(l, r);
}

#define FBCREATE(_e_op, _op) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r) { \
  assert(l->getWidth()==r->getWidth() && "type mismatch");          \
  if (FConstantExpr *cl = dyn_cast<FConstantExpr>(l))                 \
    if (FConstantExpr *cr = dyn_cast<FConstantExpr>(r))               \
      return cl->_op(cr);                                           \
  return _e_op ## _create(l, r);                                    \
}

FBCREATE(FAddExpr, FAdd)
FBCREATE(FSubExpr, FSub)
FBCREATE(FMulExpr, FMul)
FBCREATE(FDivExpr, FDiv)
FBCREATE(FRemExpr, FRem)

// N.B. The NaN semantics for these functions were taken from the following source:
//   DRAFT Standard for
//   Floating-Point Arithmetic P754
//   Draft 1.2.9
//   (section 9.2)
// and confirmed using real calculations.

FPExpr::FPCategories FAddExpr::getCategories() const {
  FPCategories lcat = left->asFPExpr()->getCategories();
  FPCategories rcat = right->asFPExpr()->getCategories();
  int cat = 0;
  if (lcat == fcAll || rcat == fcAll)
    return fcAll;
  if ((lcat & fcMaybePNorm && rcat & fcMaybeNNorm)
   || (lcat & fcMaybeNNorm && rcat & fcMaybePNorm)
   || (lcat & fcMaybeZero && rcat & fcMaybeZero))
    cat |= fcMaybeZero;
  if ((lcat & fcMaybePNorm && rcat & fcMaybePNorm)
   || (lcat & fcMaybeZero && rcat & fcMaybePNorm)
   || (lcat & fcMaybePNorm && rcat & fcMaybeZero))
    cat |= fcMaybePNorm;
  if ((lcat & fcMaybeNNorm && rcat & fcMaybeNNorm)
   || (lcat & fcMaybeZero && rcat & fcMaybeNNorm)
   || (lcat & fcMaybeNNorm && rcat & fcMaybeZero))
    cat |= fcMaybeNNorm;
  if ((lcat & fcMaybePNorm && rcat & fcMaybePNorm)
   || (lcat & fcMaybePInf && rcat & fcMaybeZero)
   || (lcat & fcMaybeZero && rcat & fcMaybePInf)
   || (lcat & fcMaybePInf && rcat & fcMaybePNorm)
   || (lcat & fcMaybePNorm && rcat & fcMaybePInf)
   || (lcat & fcMaybePInf && rcat & fcMaybeNNorm)
   || (lcat & fcMaybeNNorm && rcat & fcMaybePInf)
   || (lcat & fcMaybePInf && rcat & fcMaybePInf))
    cat |= fcMaybePInf;
  if ((lcat & fcMaybeNNorm && rcat & fcMaybeNNorm)
   || (lcat & fcMaybeNInf && rcat & fcMaybeZero)
   || (lcat & fcMaybeZero && rcat & fcMaybeNInf)
   || (lcat & fcMaybeNInf && rcat & fcMaybePNorm)
   || (lcat & fcMaybePNorm && rcat & fcMaybeNInf)
   || (lcat & fcMaybeNInf && rcat & fcMaybeNNorm)
   || (lcat & fcMaybeNNorm && rcat & fcMaybeNInf)
   || (lcat & fcMaybeNInf && rcat & fcMaybeNInf))
    cat |= fcMaybeNInf;
  if ((lcat & fcMaybeNaN)
   || (rcat & fcMaybeNaN)
   || (lcat & fcMaybePInf && rcat & fcMaybeNInf)
   || (lcat & fcMaybeNInf && rcat & fcMaybePInf))
    cat |= fcMaybeNaN;
  return (FPCategories) cat;
}

FPExpr::FPCategories FSubExpr::getCategories() const {
  FPCategories lcat = left->asFPExpr()->getCategories();
  FPCategories rcat = right->asFPExpr()->getCategories();
  int cat = 0;
  if (lcat == fcAll || rcat == fcAll)
    return fcAll;
  if ((lcat & fcMaybePNorm && rcat & fcMaybePNorm)
   || (lcat & fcMaybeNNorm && rcat & fcMaybeNNorm)
   || (lcat & fcMaybeZero && rcat & fcMaybeZero))
    cat |= fcMaybeZero;
  if ((lcat & fcMaybePNorm && rcat & fcMaybeNNorm)
   || (lcat & fcMaybeZero && rcat & fcMaybeNNorm)
   || (lcat & fcMaybePNorm && rcat & fcMaybeZero))
    cat |= fcMaybePNorm;
  if ((lcat & fcMaybeNNorm && rcat & fcMaybePNorm)
   || (lcat & fcMaybeZero && rcat & fcMaybePNorm)
   || (lcat & fcMaybeNNorm && rcat & fcMaybeZero))
    cat |= fcMaybeNNorm;
  if ((lcat & fcMaybePNorm && rcat & fcMaybeNNorm)
   || (lcat & fcMaybePInf && rcat & fcMaybeZero)
   || (lcat & fcMaybeZero && rcat & fcMaybeNInf)
   || (lcat & fcMaybePInf && rcat & fcMaybeNNorm)
   || (lcat & fcMaybePNorm && rcat & fcMaybeNInf)
   || (lcat & fcMaybePInf && rcat & fcMaybePNorm)
   || (lcat & fcMaybeNNorm && rcat & fcMaybeNInf)
   || (lcat & fcMaybePInf && rcat & fcMaybeNInf))
    cat |= fcMaybePInf;
  if ((lcat & fcMaybeNNorm && rcat & fcMaybePNorm)
   || (lcat & fcMaybeNInf && rcat & fcMaybeZero)
   || (lcat & fcMaybeZero && rcat & fcMaybePInf)
   || (lcat & fcMaybeNInf && rcat & fcMaybePNorm)
   || (lcat & fcMaybePNorm && rcat & fcMaybePInf)
   || (lcat & fcMaybeNInf && rcat & fcMaybeNNorm)
   || (lcat & fcMaybeNNorm && rcat & fcMaybePInf)
   || (lcat & fcMaybeNInf && rcat & fcMaybePInf))
    cat |= fcMaybeNInf;
  if ((lcat & fcMaybeNaN)
   || (rcat & fcMaybeNaN)
   || (lcat & fcMaybePInf && rcat & fcMaybePInf)
   || (lcat & fcMaybeNInf && rcat & fcMaybeNInf))
    cat |= fcMaybeNaN;
  return (FPCategories) cat;
}

FPExpr::FPCategories FMulExpr::getCategories() const {
  FPCategories lcat = left->asFPExpr()->getCategories();
  FPCategories rcat = right->asFPExpr()->getCategories();
  int cat = 0;
  if (lcat == fcAll || rcat == fcAll)
    return fcAll;
  if ((lcat & fcMaybeZero && rcat & (fcMaybeNNorm | fcMaybeZero | fcMaybePNorm))
   || (rcat & fcMaybeZero && lcat & (fcMaybeNNorm | fcMaybeZero | fcMaybePNorm)))
    cat |= fcMaybeZero;
  if ((lcat & fcMaybePNorm && rcat & fcMaybePNorm)
   || (lcat & fcMaybeNNorm && rcat & fcMaybeNNorm))
    cat |= fcMaybePNorm;
  if ((lcat & fcMaybePNorm && rcat & fcMaybeNNorm)
   || (lcat & fcMaybeNNorm && rcat & fcMaybePNorm))
    cat |= fcMaybeNNorm;
  if ((lcat & (fcMaybePNorm | fcMaybePInf) && rcat & (fcMaybePNorm | fcMaybePInf))
   || (lcat & (fcMaybeNNorm | fcMaybeNInf) && rcat & (fcMaybeNNorm | fcMaybeNInf)))
    cat |= fcMaybePInf;
  if ((lcat & (fcMaybePNorm | fcMaybePInf) && rcat & (fcMaybeNNorm | fcMaybeNInf))
   || (lcat & (fcMaybeNNorm | fcMaybeNInf) && rcat & (fcMaybePNorm | fcMaybePInf)))
    cat |= fcMaybeNInf;
  if ((lcat & fcMaybeNaN)
   || (rcat & fcMaybeNaN)
   || (lcat & fcMaybeZero && rcat & (fcMaybeNInf | fcMaybePInf))
   || (rcat & fcMaybeZero && lcat & (fcMaybeNInf | fcMaybePInf)))
    cat |= fcMaybeNaN;
  return (FPCategories) cat;
}

FPExpr::FPCategories FDivExpr::getCategories() const {
  FPCategories lcat = left->asFPExpr()->getCategories();
  FPCategories rcat = right->asFPExpr()->getCategories();
  int cat = 0;
  if (lcat == fcAll || rcat == fcAll)
    return fcAll;
  if (lcat & (fcMaybeNNorm | fcMaybeZero | fcMaybePNorm) && rcat & (fcMaybeNNorm | fcMaybePNorm))
    cat |= fcMaybeZero;
  if ((lcat & fcMaybePNorm && rcat & fcMaybePNorm)
   || (lcat & fcMaybeNNorm && rcat & fcMaybeNNorm))
    cat |= fcMaybePNorm;
  if ((lcat & fcMaybePNorm && rcat & fcMaybeNNorm)
   || (lcat & fcMaybeNNorm && rcat & fcMaybePNorm))
    cat |= fcMaybeNNorm;
  if ((lcat & (fcMaybePNorm | fcMaybePInf) && rcat & (fcMaybePNorm | fcMaybeZero))
   || (lcat & (fcMaybeNNorm | fcMaybeNInf) && rcat & (fcMaybeNNorm | fcMaybeZero)))
    cat |= fcMaybePInf;
  if ((lcat & (fcMaybePNorm | fcMaybePInf) && rcat & (fcMaybeNNorm | fcMaybeZero))
   || (lcat & (fcMaybeNNorm | fcMaybeNInf) && rcat & (fcMaybePNorm | fcMaybeZero)))
    cat |= fcMaybeNInf;
  if ((lcat & fcMaybeNaN)
   || (rcat & fcMaybeNaN)
   || (lcat & fcMaybeZero && rcat & fcMaybeZero)
   || (rcat & (fcMaybeNInf | fcMaybePInf) && lcat & (fcMaybeNInf | fcMaybePInf)))
    cat |= fcMaybeNaN;
  return (FPCategories) cat;
}

FPExpr::FPCategories FRemExpr::getCategories() const {
  return fcAll; // TODO
}

FPExpr::FPCategories FPExtExpr::getCategories() const {
  return src->asFPExpr()->getCategories();
}

FPExpr::FPCategories FPTruncExpr::getCategories() const {
  int cat = src->asFPExpr()->getCategories();
  if (cat & fcMaybePNorm)
    cat |= fcMaybePInf;
  if (cat & fcMaybeNNorm)
    cat |= fcMaybeNInf;
  return (FPCategories) cat;
}

/* An interesting addition for the I->F cases would be to use STP
 * to determine which categories the result falls into */

FPExpr::FPCategories UIToFPExpr::getCategories() const {
  return (FPCategories) (fcMaybeNNorm | fcMaybeZero | fcMaybePNorm);
}

FPExpr::FPCategories SIToFPExpr::getCategories() const {
  return (FPCategories) (fcMaybeZero | fcMaybePNorm);
}

#define CMPCREATE(_e_op, _op) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r) { \
  assert(l->getWidth()==r->getWidth() && "type mismatch");              \
  if (IConstantExpr *cl = dyn_cast<IConstantExpr>(l))                     \
    if (IConstantExpr *cr = dyn_cast<IConstantExpr>(r))                   \
      return cl->_op(cr);                                               \
  return _e_op ## _create(l, r);                                        \
}

#define CMPCREATE_T(_e_op, _op, _reflexive_e_op, partialL, partialR) \
ref<Expr>  _e_op ::create(const ref<Expr> &l, const ref<Expr> &r) {    \
  assert(l->getWidth()==r->getWidth() && "type mismatch");             \
  if (IConstantExpr *cl = dyn_cast<IConstantExpr>(l)) {                  \
    if (IConstantExpr *cr = dyn_cast<IConstantExpr>(r))                  \
      return cl->_op(cr);                                              \
    return partialR(cl, r.get());                                      \
  } else if (IConstantExpr *cr = dyn_cast<IConstantExpr>(r)) {           \
    return partialL(l.get(), cr);                                      \
  } else {                                                             \
    return _e_op ## _create(l.get(), r.get());                         \
  }                                                                    \
}
  

static ref<Expr> EqExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l == r) {
    return IConstantExpr::alloc(1, Expr::Bool);
  } else {
    return EqExpr::alloc(l, r);
  }
}


/// Tries to optimize EqExpr cl == rd, where cl is a IConstantExpr and
/// rd a ReadExpr.  If rd is a read into an all-constant array,
/// returns a disjunction of equalities on the index.  Otherwise,
/// returns the initial equality expression. 
static ref<Expr> TryConstArrayOpt(const ref<ConstantExpr> &cl, 
				  ReadExpr *rd) {
  if (rd->updates.root->isSymbolicArray() || rd->updates.getSize())
    return EqExpr_create(cl, rd);

  // Number of positions in the array that contain value ct.
  unsigned numMatches = 0;

  // for now, just assume standard "flushing" of a concrete array,
  // where the concrete array has one update for each index, in order
  ref<Expr> res = IConstantExpr::alloc(0, Expr::Bool);
  for (unsigned i = 0, e = rd->updates.root->size; i != e; ++i) {
    if (cl == rd->updates.root->constantValues[i]) {
      // Arbitrary maximum on the size of disjunction.
      if (++numMatches > 100)
        return EqExpr_create(cl, rd);
      
      ref<Expr> mayBe = 
        EqExpr::create(rd->index, IConstantExpr::alloc(i, 
                                                      rd->index->getWidth()));
      res = OrExpr::create(res, mayBe);
    }
  }

  return res;
}

static ref<Expr> EqExpr_createPartialR(const ref<IConstantExpr> &cl, Expr *r) {  
  Expr::Width width = cl->getWidth();

  Expr::Kind rk = r->getKind();
  if (width == Expr::Bool) {
    if (cl->isTrue()) {
      return r;
    } else {
      // 0 == ...
      
      if (rk == Expr::Eq) {
        const EqExpr *ree = cast<EqExpr>(r);

        // eliminate double negation
        if (IConstantExpr *CE = dyn_cast<IConstantExpr>(ree->left)) {
          // 0 == (0 == A) => A
          if (CE->getWidth() == Expr::Bool &&
              CE->isFalse())
            return ree->right;
        }
      } else if (rk == Expr::Or) {
        const OrExpr *roe = cast<OrExpr>(r);

        // transform not(or(a,b)) to and(not a, not b)
        return AndExpr::create(Expr::createIsZero(roe->left),
                               Expr::createIsZero(roe->right));
      }
    }
  } else if (rk == Expr::SExt) {
    // (sext(a,T)==c) == (a==c)
    const SExtExpr *see = cast<SExtExpr>(r);
    Expr::Width fromBits = see->src->getWidth();
    ref<IConstantExpr> trunc = cl->ZExt(fromBits);

    // pathological check, make sure it is possible to
    // sext to this value *from any value*
    if (cl == trunc->SExt(width)) {
      return EqExpr::create(see->src, trunc);
    } else {
      return IConstantExpr::create(0, Expr::Bool);
    }
  } else if (rk == Expr::ZExt) {
    // (zext(a,T)==c) == (a==c)
    const ZExtExpr *zee = cast<ZExtExpr>(r);
    Expr::Width fromBits = zee->src->getWidth();
    ref<IConstantExpr> trunc = cl->ZExt(fromBits);
    
    // pathological check, make sure it is possible to
    // zext to this value *from any value*
    if (cl == trunc->ZExt(width)) {
      return EqExpr::create(zee->src, trunc);
    } else {
      return IConstantExpr::create(0, Expr::Bool);
    }
  } else if (rk==Expr::Add) {
    const AddExpr *ae = cast<AddExpr>(r);
    if (isa<IConstantExpr>(ae->left)) {
      // c0 = c1 + b => c0 - c1 = b
      return EqExpr_createPartialR(cast<IConstantExpr>(SubExpr::create(cl, 
                                                                      ae->left)),
                                   ae->right.get());
    }
  } else if (rk==Expr::Sub) {
    const SubExpr *se = cast<SubExpr>(r);
    if (isa<IConstantExpr>(se->left)) {
      // c0 = c1 - b => c1 - c0 = b
      return EqExpr_createPartialR(cast<IConstantExpr>(SubExpr::create(se->left, 
                                                                      cl)),
                                   se->right.get());
    }
  } else if (rk == Expr::Read && ConstArrayOpt) {
    return TryConstArrayOpt(cl, static_cast<ReadExpr*>(r));
  }
    
  return EqExpr_create(cl, r);
}

static ref<Expr> EqExpr_createPartial(Expr *l, const ref<IConstantExpr> &cr) {  
  return EqExpr_createPartialR(cr, l);
}
  
ref<Expr> NeExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return EqExpr::create(IConstantExpr::create(0, Expr::Bool),
                        EqExpr::create(l, r));
}

ref<Expr> UgtExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return UltExpr::create(r, l);
}
ref<Expr> UgeExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return UleExpr::create(r, l);
}

ref<Expr> SgtExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return SltExpr::create(r, l);
}
ref<Expr> SgeExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return SleExpr::create(r, l);
}

static ref<Expr> UltExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  Expr::Width t = l->getWidth();
  if (t == Expr::Bool) { // !l && r
    return AndExpr::create(Expr::createIsZero(l), r);
  } else {
    return UltExpr::alloc(l, r);
  }
}

static ref<Expr> UleExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // !(l && !r)
    return OrExpr::create(Expr::createIsZero(l), r);
  } else {
    return UleExpr::alloc(l, r);
  }
}

static ref<Expr> SltExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // l && !r
    return AndExpr::create(l, Expr::createIsZero(r));
  } else {
    return SltExpr::alloc(l, r);
  }
}

static ref<Expr> SleExpr_create(const ref<Expr> &l, const ref<Expr> &r) {
  if (l->getWidth() == Expr::Bool) { // !(!l && r)
    return OrExpr::create(l, Expr::createIsZero(r));
  } else {
    return SleExpr::alloc(l, r);
  }
}

CMPCREATE_T(EqExpr, Eq, EqExpr, EqExpr_createPartial, EqExpr_createPartialR)
CMPCREATE(UltExpr, Ult)
CMPCREATE(UleExpr, Ule)
CMPCREATE(SltExpr, Slt)
CMPCREATE(SleExpr, Sle)

ref<Expr> FOeqExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  if (FConstantExpr *cl = dyn_cast<FConstantExpr>(l)) {
    if (FConstantExpr *cr = dyn_cast<FConstantExpr>(r))
      return cl->FOeq(cr);

    // NaN == X is always false
    const APFloat &lv = cl->getAPValue();
    if (lv.isNaN())
      return IConstantExpr::create(0, Expr::Bool);
  }
  else if (FConstantExpr *cr = dyn_cast<FConstantExpr>(r)) {
    // likewise X == NaN
    const APFloat &rv = cr->getAPValue();
    if (rv.isNaN())
      return IConstantExpr::create(0, Expr::Bool);
  }

  return FOeqExpr::alloc(l, r);
}

ref<Expr> FOltExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  if (FConstantExpr *cl = dyn_cast<FConstantExpr>(l)) {
    if (FConstantExpr *cr = dyn_cast<FConstantExpr>(r))
      return cl->FOlt(cr);

    // inf < X and NaN < X are always false
    const APFloat &lv = cl->getAPValue();
    if ((lv.isInfinity() && !lv.isNegative()) || lv.isNaN())
      return IConstantExpr::create(0, Expr::Bool);
  }
  else if (FConstantExpr *cr = dyn_cast<FConstantExpr>(r)) {
    // likewise X < -inf and X < NaN
    const APFloat &rv = cr->getAPValue();
    if ((rv.isInfinity() && rv.isNegative()) || rv.isNaN())
      return IConstantExpr::create(0, Expr::Bool);
  }

  return FOltExpr::alloc(l, r);
}

ref<Expr> FOleExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return OrExpr::create(FOltExpr::create(l, r), FOeqExpr::create(l, r));
}

ref<Expr> FOgtExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return FOltExpr::create(r, l);
}

ref<Expr> FOgeExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return FOleExpr::create(r, l);
}

ref<Expr> FOneExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return OrExpr::create(FOltExpr::create(l, r), FOgtExpr::create(l, r));
}

ref<Expr> FOrd1Expr::create(const ref<Expr> &e) {
  if (FConstantExpr *ce = dyn_cast<FConstantExpr>(e))
    return ce->FOrd();

  return FOrd1Expr::alloc(e);
}

ref<Expr> FOrdExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return OrExpr::create(FOrd1Expr::create(l), FOrd1Expr::create(r));
}

ref<Expr> FUeqExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return NotExpr::create(FOneExpr::create(l, r));
}

ref<Expr> FUltExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return NotExpr::create(FOgeExpr::create(l, r));
}

ref<Expr> FUleExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return NotExpr::create(FOgtExpr::create(l, r));
}

ref<Expr> FUgtExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return NotExpr::create(FOleExpr::create(l, r));
}

ref<Expr> FUgeExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return NotExpr::create(FOltExpr::create(l, r));
}

ref<Expr> FUneExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return NotExpr::create(FOeqExpr::create(l, r));
}

ref<Expr> FUnoExpr::create(const ref<Expr> &l, const ref<Expr> &r) {
  return NotExpr::create(FOrdExpr::create(l, r));
}
