//===-- STPBuilder.cpp ----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "STPBuilder.h"

#include "klee/Expr.h"
#include "klee/Solver.h"
#include "klee/util/Bits.h"

#include "ConstantDivision.h"
#include "SolverStats.h"

#include "llvm/Support/CommandLine.h"

#include <cstdio>

#define vc_bvBoolExtract IAMTHESPAWNOFSATAN
// unclear return
#define vc_bvLeftShiftExpr IAMTHESPAWNOFSATAN
#define vc_bvRightShiftExpr IAMTHESPAWNOFSATAN
// bad refcnt'ng
#define vc_bvVar32LeftShiftExpr IAMTHESPAWNOFSATAN
#define vc_bvVar32RightShiftExpr IAMTHESPAWNOFSATAN
#define vc_bvVar32DivByPowOfTwoExpr IAMTHESPAWNOFSATAN
#define vc_bvCreateMemoryArray IAMTHESPAWNOFSATAN
#define vc_bvReadMemoryArray IAMTHESPAWNOFSATAN
#define vc_bvWriteToMemoryArray IAMTHESPAWNOFSATAN

#include <algorithm> // max, min
#include <cassert>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

using namespace klee;

namespace {
  llvm::cl::opt<bool>
  UseConstructHash("use-construct-hash", 
                   llvm::cl::desc("Use hash-consing during STP query construction."),
                   llvm::cl::init(true));
}

///

/***/

STPBuilder::STPBuilder(::VC _vc, bool _optimizeDivides) 
  : vc(_vc), optimizeDivides(_optimizeDivides), fpCount(0)
{
  tempVars[0] = buildVar("__tmpInt8", 8);
  tempVars[1] = buildVar("__tmpInt16", 16);
  tempVars[2] = buildVar("__tmpInt32", 32);
  tempVars[3] = buildVar("__tmpInt64", 64);
}

STPBuilder::~STPBuilder() {
}

///

/* Warning: be careful about what c_interface functions you use. Some of
   them look like they cons memory but in fact don't, which is bad when
   you call vc_DeleteExpr on them. */

::VCExpr STPBuilder::buildVar(const char *name, unsigned width) {
  // XXX don't rebuild if this stuff cons's
  ::Type t = (width==1) ? vc_boolType(vc) : vc_bvType(vc, width);
  ::VCExpr res = vc_varExpr(vc, const_cast<char*>(name), t);
  vc_DeleteExpr(t);
  return res;
}

::VCExpr STPBuilder::buildArray(const char *name, unsigned indexWidth, unsigned valueWidth) {
  // XXX don't rebuild if this stuff cons's
  ::Type t1 = vc_bvType(vc, indexWidth);
  ::Type t2 = vc_bvType(vc, valueWidth);
  ::Type t = vc_arrayType(vc, t1, t2);
  ::VCExpr res = vc_varExpr(vc, const_cast<char*>(name), t);
  vc_DeleteExpr(t);
  vc_DeleteExpr(t2);
  vc_DeleteExpr(t1);
  return res;
}

ExprHandle STPBuilder::getTempVar(Expr::Width w) {
  switch (w) {
  default: assert(0 && "invalid type");
  case Expr::Int8: return tempVars[0];
  case Expr::Int16: return tempVars[1];
  case Expr::Int32: return tempVars[2];
  case Expr::Int64: return tempVars[3];
  }
}

ExprHandle STPBuilder::getTrue() {
  return vc_trueExpr(vc);
}
ExprHandle STPBuilder::getFalse() {
  return vc_falseExpr(vc);
}
ExprHandle STPBuilder::bvOne(unsigned width) {
  return bvConst32(width, 1);
}
ExprHandle STPBuilder::bvZero(unsigned width) {
  return bvConst32(width, 0);
}
ExprHandle STPBuilder::bvMinusOne(unsigned width) {
  return bvConst64(width, (int64_t) -1);
}
ExprHandle STPBuilder::bvConst32(unsigned width, uint32_t value) {
  return vc_bvConstExprFromInt(vc, width, value);
}
ExprHandle STPBuilder::bvConst64(unsigned width, uint64_t value) {
  return vc_bvConstExprFromLL(vc, width, value);
}

ExprHandle STPBuilder::bvBoolExtract(ExprHandle expr, int bit) {
  return vc_eqExpr(vc, bvExtract(expr, bit, bit), bvOne(1));
}
ExprHandle STPBuilder::bvExtract(ExprHandle expr, unsigned top, unsigned bottom) {
  return vc_bvExtract(vc, expr, top, bottom);
}
ExprHandle STPBuilder::eqExpr(ExprHandle a, ExprHandle b) {
  return vc_eqExpr(vc, a, b);
}

// logical right shift
ExprHandle STPBuilder::bvRightShift(ExprHandle expr, unsigned amount, unsigned shiftBits) {
  unsigned width = vc_getBVLength(vc, expr);
  unsigned shift = amount & ((1<<shiftBits) - 1);

  if (shift==0) {
    return expr;
  } else if (shift>=width) {
    return bvZero(width);
  } else {
    return vc_bvConcatExpr(vc,
                           bvZero(shift),
                           bvExtract(expr, width - 1, shift));
  }
}

// logical left shift
ExprHandle STPBuilder::bvLeftShift(ExprHandle expr, unsigned amount, unsigned shiftBits) {
  unsigned width = vc_getBVLength(vc, expr);
  unsigned shift = amount & ((1<<shiftBits) - 1);

  if (shift==0) {
    return expr;
  } else if (shift>=width) {
    return bvZero(width);
  } else {
    // stp shift does "expr @ [0 x s]" which we then have to extract,
    // rolling our own gives slightly smaller exprs
    return vc_bvConcatExpr(vc, 
                           bvExtract(expr, width - shift - 1, 0),
                           bvZero(shift));
  }
}

// left shift by a variable amount on an expression of the specified width
ExprHandle STPBuilder::bvVarLeftShift(ExprHandle expr, ExprHandle amount, unsigned width) {
  ExprHandle res = bvZero(width);

  int shiftBits = getShiftBits( width );

  //get the shift amount (looking only at the bits appropriate for the given width)
  ExprHandle shift = vc_bvExtract( vc, amount, shiftBits - 1, 0 ); 

  //construct a big if-then-elif-elif-... with one case per possible shift amount
  for( int i=width-1; i>=0; i-- ) {
    res = vc_iteExpr(vc,
                     eqExpr(shift, bvConst32(shiftBits, i)),
                     bvLeftShift(expr, i, shiftBits),
                     res);
  }
  return res;
}

// logical right shift by a variable amount on an expression of the specified width
ExprHandle STPBuilder::bvVarRightShift(ExprHandle expr, ExprHandle amount, unsigned width) {
  ExprHandle res = bvZero(width);

  int shiftBits = getShiftBits( width );

  //get the shift amount (looking only at the bits appropriate for the given width)
  ExprHandle shift = vc_bvExtract( vc, amount, shiftBits - 1, 0 ); 

  //construct a big if-then-elif-elif-... with one case per possible shift amount
  for( int i=width-1; i>=0; i-- ) {
    res = vc_iteExpr(vc,
                     eqExpr(shift, bvConst32(shiftBits, i)),
                     bvRightShift(expr, i, shiftBits),
                     res);
  }

  return res;
}

// arithmetic right shift by a variable amount on an expression of the specified width
ExprHandle STPBuilder::bvVarArithRightShift(ExprHandle expr, ExprHandle amount, unsigned width) {
  int shiftBits = getShiftBits( width );

  //get the shift amount (looking only at the bits appropriate for the given width)
  ExprHandle shift = vc_bvExtract( vc, amount, shiftBits - 1, 0 );

  //get the sign bit to fill with
  ExprHandle signedBool = bvBoolExtract(expr, width-1);

  //start with the result if shifting by width-1
  ExprHandle res = constructAShrByConstant(expr, width-1, signedBool, shiftBits);

  //construct a big if-then-elif-elif-... with one case per possible shift amount
  // XXX more efficient to move the ite on the sign outside all exprs?
  // XXX more efficient to sign extend, right shift, then extract lower bits?
  for( int i=width-2; i>=0; i-- ) {
    res = vc_iteExpr(vc,
                     eqExpr(shift, bvConst32(shiftBits,i)),
                     constructAShrByConstant(expr, 
                                             i, 
                                             signedBool, 
                                             shiftBits),
                     res);
  }

  return res;
}

ExprHandle STPBuilder::constructAShrByConstant(ExprHandle expr,
                                               unsigned amount,
                                               ExprHandle isSigned, 
                                               unsigned shiftBits) {
  unsigned width = vc_getBVLength(vc, expr);
  unsigned shift = amount & ((1<<shiftBits) - 1);

  if (shift==0) {
    return expr;
  } else if (shift>=width-1) {
    return vc_iteExpr(vc, isSigned, bvMinusOne(width), bvZero(width));
  } else {
    return vc_iteExpr(vc,
                      isSigned,
                      ExprHandle(vc_bvConcatExpr(vc,
                                                 bvMinusOne(shift),
                                                 bvExtract(expr, width - 1, shift))),
                      bvRightShift(expr, shift, shiftBits));
  }
}

ExprHandle STPBuilder::constructMulByConstant(ExprHandle expr, unsigned width, uint64_t x) {
  unsigned shiftBits = getShiftBits(width);
  uint64_t add, sub;
  ExprHandle res = 0;

  // expr*x == expr*(add-sub) == expr*add - expr*sub
  ComputeMultConstants64(x, add, sub);

  // legal, these would overflow completely
  add = bits64::truncateToNBits(add, width);
  sub = bits64::truncateToNBits(sub, width);

  for (int j=63; j>=0; j--) {
    uint64_t bit = 1LL << j;

    if ((add&bit) || (sub&bit)) {
      assert(!((add&bit) && (sub&bit)) && "invalid mult constants");
      ExprHandle op = bvLeftShift(expr, j, shiftBits);
      
      if (add&bit) {
        if (res) {
          res = vc_bvPlusExpr(vc, width, res, op);
        } else {
          res = op;
        }
      } else {
        if (res) {
          res = vc_bvMinusExpr(vc, width, res, op);
        } else {
          res = vc_bvUMinusExpr(vc, op);
        }
      }
    }
  }

  if (!res) 
    res = bvZero(width);

  return res;
}

/* 
 * Compute the 32-bit unsigned integer division of n by a divisor d based on 
 * the constants derived from the constant divisor d.
 *
 * Returns n/d without doing explicit division.  The cost is 2 adds, 3 shifts, 
 * and a (64-bit) multiply.
 *
 * @param n      numerator (dividend) as an expression
 * @param width  number of bits used to represent the value
 * @param d      the divisor
 *
 * @return n/d without doing explicit division
 */
ExprHandle STPBuilder::constructUDivByConstant(ExprHandle expr_n, unsigned width, uint64_t d) {
  assert(width==32 && "can only compute udiv constants for 32-bit division");

  // Compute the constants needed to compute n/d for constant d w/o
  // division by d.
  uint32_t mprime, sh1, sh2;
  ComputeUDivConstants32(d, mprime, sh1, sh2);
  ExprHandle expr_sh1    = bvConst32( 32, sh1);
  ExprHandle expr_sh2    = bvConst32( 32, sh2);

  // t1  = MULUH(mprime, n) = ( (uint64_t)mprime * (uint64_t)n ) >> 32
  ExprHandle expr_n_64   = vc_bvConcatExpr( vc, bvZero(32), expr_n ); //extend to 64 bits
  ExprHandle t1_64bits   = constructMulByConstant( expr_n_64, 64, (uint64_t)mprime );
  ExprHandle t1          = vc_bvExtract( vc, t1_64bits, 63, 32 ); //upper 32 bits

  // n/d = (((n - t1) >> sh1) + t1) >> sh2;
  ExprHandle n_minus_t1  = vc_bvMinusExpr( vc, width, expr_n, t1 );
  ExprHandle shift_sh1   = bvVarRightShift( n_minus_t1, expr_sh1, 32 );
  ExprHandle plus_t1     = vc_bvPlusExpr( vc, width, shift_sh1, t1 );
  ExprHandle res         = bvVarRightShift( plus_t1, expr_sh2, 32 );

  return res;
}

/* 
 * Compute the 32-bitnsigned integer division of n by a divisor d based on 
 * the constants derived from the constant divisor d.
 *
 * Returns n/d without doing explicit division.  The cost is 3 adds, 3 shifts, 
 * a (64-bit) multiply, and an XOR.
 *
 * @param n      numerator (dividend) as an expression
 * @param width  number of bits used to represent the value
 * @param d      the divisor
 *
 * @return n/d without doing explicit division
 */
ExprHandle STPBuilder::constructSDivByConstant(ExprHandle expr_n, unsigned width, uint64_t d) {
  assert(width==32 && "can only compute udiv constants for 32-bit division");

  // Compute the constants needed to compute n/d for constant d w/o division by d.
  int32_t mprime, dsign, shpost;
  ComputeSDivConstants32(d, mprime, dsign, shpost);
  ExprHandle expr_dsign   = bvConst32( 32, dsign);
  ExprHandle expr_shpost  = bvConst32( 32, shpost);

  // q0 = n + MULSH( mprime, n ) = n + (( (int64_t)mprime * (int64_t)n ) >> 32)
  int64_t mprime_64     = (int64_t)mprime;

  ExprHandle expr_n_64    = vc_bvSignExtend( vc, expr_n, 64 );
  ExprHandle mult_64      = constructMulByConstant( expr_n_64, 64, mprime_64 );
  ExprHandle mulsh        = vc_bvExtract( vc, mult_64, 63, 32 ); //upper 32-bits
  ExprHandle n_plus_mulsh = vc_bvPlusExpr( vc, width, expr_n, mulsh );

  // Improved variable arithmetic right shift: sign extend, shift,
  // extract.
  ExprHandle extend_npm   = vc_bvSignExtend( vc, n_plus_mulsh, 64 );
  ExprHandle shift_npm    = bvVarRightShift( extend_npm, expr_shpost, 64 );
  ExprHandle shift_shpost = vc_bvExtract( vc, shift_npm, 31, 0 ); //lower 32-bits

  // XSIGN(n) is -1 if n is negative, positive one otherwise
  ExprHandle is_signed    = bvBoolExtract( expr_n, 31 );
  ExprHandle neg_one      = bvMinusOne(32);
  ExprHandle xsign_of_n   = vc_iteExpr( vc, is_signed, neg_one, bvZero(32) );

  // q0 = (n_plus_mulsh >> shpost) - XSIGN(n)
  ExprHandle q0           = vc_bvMinusExpr( vc, width, shift_shpost, xsign_of_n );
  
  // n/d = (q0 ^ dsign) - dsign
  ExprHandle q0_xor_dsign = vc_bvXorExpr( vc, q0, expr_dsign );
  ExprHandle res          = vc_bvMinusExpr( vc, width, q0_xor_dsign, expr_dsign );

  return res;
}

::VCExpr STPBuilder::getInitialArray(const Array *root) {
  if (root->stpInitialArray) {
    return root->stpInitialArray;
  } else {
    // STP uniques arrays by name, so we make sure the name is unique by
    // including the address.
    char buf[32];
    sprintf(buf, "%s_%p", root->name.c_str(), (void*) root);
    root->stpInitialArray = buildArray(buf, 32, 8);

    if (root->isConstantArray()) {
      // FIXME: Flush the concrete values into STP. Ideally we would do this
      // using assertions, which is much faster, but we need to fix the caching
      // to work correctly in that case.
      for (unsigned i = 0, e = root->size; i != e; ++i) {
        ::VCExpr prev = root->stpInitialArray;
        root->stpInitialArray = 
          vc_writeExpr(vc, prev,
                       construct(ConstantExpr::alloc(i, root->getDomain()), 0, etBV),
                       construct(root->constantValues[i], 0, etBV));
        vc_DeleteExpr(prev);
      }
    }

    return root->stpInitialArray;
  }
}

ExprHandle STPBuilder::getInitialRead(const Array *root, unsigned index) {
  return vc_readExpr(vc, getInitialArray(root), bvConst32(32, index));
}

::VCExpr STPBuilder::getArrayForUpdate(const Array *root, 
                                       const UpdateNode *un) {
  if (!un) {
    return getInitialArray(root);
  } else {
    // FIXME: This really needs to be non-recursive.
    if (!un->stpArray)
      un->stpArray = vc_writeExpr(vc,
                                  getArrayForUpdate(root, un->next),
                                  construct(un->index, 0, etBV),
                                  construct(un->value, 0, etBV));

    return un->stpArray;
  }
}


/** if *et_out==etBV then result is a bitvector,
    otherwise it is a bool */
ExprHandle STPBuilder::construct(ref<Expr> e, int *width_out, STPExprType *et_out) {
  if (!UseConstructHash || isa<ConstantExpr>(e)) {
    return constructActual(e, width_out, et_out);
  } else {
    ExprHashMap<ConstructedExpr>::iterator it = constructed.find(e);
    if (it!=constructed.end()) {
      if (width_out)
        *width_out = it->second.width;
      if (et_out)
        *et_out = it->second.type;
      return it->second.expr;
    } else {
      int width;
      if (!width_out) width_out = &width;
      ExprHandle res = constructActual(e, width_out, et_out);
      constructed.insert(std::make_pair(e, ConstructedExpr(res, *width_out, *et_out)));
      return res;
    }
  }
}

ExprHandle STPBuilder::construct(ref<Expr> e, int *width_out, STPExprType et_out) {
  STPExprType et_in = et_out;
  assert(et_out != etDontCare);
  ExprHandle expr = construct(e, width_out, &et_in);
  assert(et_in != etDontCare);
  if (et_in == etBOOL && et_out == etBV) {
    expr = vc_iteExpr(vc, expr, bvOne(1), bvZero(1));
  } else if (et_in == etBV && et_out == etBOOL) {
    expr = bvBoolExtract(expr, 0);
  }
  return expr;
}

/** if *et_out==etBV then result is a bitvector,
    otherwise it is a bool */
ExprHandle STPBuilder::constructActual(ref<Expr> e, int *width_out, STPExprType *et_out) {
  int width;
  if (!width_out) width_out = &width;

  ++stats::queryConstructs;

  switch (e->getKind()) {
  case Expr::Constant: {
    ConstantExpr *CE = cast<ConstantExpr>(e);
    *width_out = CE->getWidth();

    // Coerce to bool if necessary.
    if (*width_out == 1 && (*et_out == etBOOL || *et_out == etDontCare)) {
      *et_out = etBOOL;
      return CE->isTrue() ? getTrue() : getFalse();
    }

    *et_out = etBV;

    // Fast path.
    if (*width_out <= 32)
      return bvConst32(*width_out, CE->getZExtValue(32));
    if (*width_out <= 64)
      return bvConst64(*width_out, CE->getZExtValue());

    // FIXME: Optimize?
    ref<ConstantExpr> Tmp = CE;
    ExprHandle Res = bvConst64(64, Tmp->Extract(0, 64)->getZExtValue());
    for (unsigned i = (*width_out / 64) - 1; i; --i) {
      Tmp = Tmp->LShr(ConstantExpr::alloc(64, Tmp->getWidth()));
      Res = vc_bvConcatExpr(vc, bvConst64(std::min(64U, Tmp->getWidth()),
                                          Tmp->Extract(0, 64)->getZExtValue()),
                            Res);
    }
    return Res;
  }
    
  // Special
  case Expr::NotOptimized: {
    NotOptimizedExpr *noe = cast<NotOptimizedExpr>(e);
    return construct(noe->src, width_out, et_out);
  }

  case Expr::Read: {
    ReadExpr *re = cast<ReadExpr>(e);
    *width_out = 8;
    *et_out = etBV;
    return vc_readExpr(vc,
                       getArrayForUpdate(re->updates.root, re->updates.head),
                       construct(re->index, 0, etBV));
  }
    
  case Expr::Select: {
    SelectExpr *se = cast<SelectExpr>(e);
    ExprHandle cond = construct(se->cond, 0, etBOOL);
    ExprHandle tExpr = construct(se->trueExpr, width_out, etBV);
    ExprHandle fExpr = construct(se->falseExpr, width_out, etBV);
    *et_out = etBV;
    return vc_iteExpr(vc, cond, tExpr, fExpr);
  }

  case Expr::Concat: {
    ConcatExpr *ce = cast<ConcatExpr>(e);
    unsigned numKids = ce->getNumKids();
    ExprHandle res = construct(ce->getKid(numKids-1), 0, etBV);
    for (int i=numKids-2; i>=0; i--) {
      res = vc_bvConcatExpr(vc, construct(ce->getKid(i), 0, etBV), res);
    }
    *width_out = ce->getWidth();
    *et_out = etBV;
    return res;
  }

  case Expr::Extract: {
    ExtractExpr *ee = cast<ExtractExpr>(e);
    ExprHandle src = construct(ee->expr, width_out, etBV);    
    *width_out = ee->getWidth();
    *et_out = etBV;
    return vc_bvExtract(vc, src, ee->offset + *width_out - 1, ee->offset);
  }

    // Casting

  case Expr::ZExt: {
    int srcWidth;
    CastExpr *ce = cast<CastExpr>(e);
    STPExprType src_rt = etDontCare;
    ExprHandle src = construct(ce->src, &srcWidth, &src_rt);
    *width_out = ce->getWidth();
    *et_out = etBV;
    if (src_rt==etBOOL) {
      return vc_iteExpr(vc, src, bvOne(*width_out), bvZero(*width_out));
    } else {
      return vc_bvConcatExpr(vc, bvZero(*width_out-srcWidth), src);
    }
  }

  case Expr::SExt: {
    int srcWidth;
    CastExpr *ce = cast<CastExpr>(e);
    STPExprType src_rt = etDontCare;
    ExprHandle src = construct(ce->src, &srcWidth, &src_rt);
    *width_out = ce->getWidth();
    *et_out = etBV;
    if (src_rt==etBOOL) {
      return vc_iteExpr(vc, src, bvMinusOne(*width_out), bvZero(*width_out));
    } else {
      return vc_bvSignExtend(vc, src, *width_out);
    }
  }

    // Arithmetic

  case Expr::Add: {
    AddExpr *ae = cast<AddExpr>(e);
    ExprHandle left = construct(ae->left, width_out, etBV);
    ExprHandle right = construct(ae->right, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized add");
    *et_out = etBV;
    return vc_bvPlusExpr(vc, *width_out, left, right);
  }

  case Expr::Sub: {
    SubExpr *se = cast<SubExpr>(e);
    ExprHandle left = construct(se->left, width_out, etBV);
    ExprHandle right = construct(se->right, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized sub");
    *et_out = etBV;
    return vc_bvMinusExpr(vc, *width_out, left, right);
  } 

  case Expr::Mul: {
    MulExpr *me = cast<MulExpr>(e);
    ExprHandle right = construct(me->right, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized mul");
    *et_out = etBV;

    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(me->left))
      if (CE->getWidth() <= 64)
        return constructMulByConstant(right, *width_out, 
                                      CE->getZExtValue());

    ExprHandle left = construct(me->left, width_out, etBV);
    return vc_bvMultExpr(vc, *width_out, left, right);
  }

  case Expr::UDiv: {
    UDivExpr *de = cast<UDivExpr>(e);
    ExprHandle left = construct(de->left, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized udiv");
    *et_out = etBV;
    
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(de->right)) {
      if (CE->getWidth() <= 64) {
        uint64_t divisor = CE->getZExtValue();
      
        if (bits64::isPowerOfTwo(divisor)) {
          return bvRightShift(left,
                              bits64::indexOfSingleBit(divisor),
                              getShiftBits(*width_out));
        } else if (optimizeDivides) {
          if (*width_out == 32) //only works for 32-bit division
            return constructUDivByConstant( left, *width_out, 
                                            (uint32_t) divisor);
        }
      }
    } 

    ExprHandle right = construct(de->right, width_out, etBV);
    return vc_bvDivExpr(vc, *width_out, left, right);
  }

  case Expr::SDiv: {
    SDivExpr *de = cast<SDivExpr>(e);
    ExprHandle left = construct(de->left, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized sdiv");
    *et_out = etBV;

    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(de->right))
      if (optimizeDivides)
	if (*width_out == 32) //only works for 32-bit division
	  return constructSDivByConstant( left, *width_out, 
                                          CE->getZExtValue(32));

    // XXX need to test for proper handling of sign, not sure I
    // trust STP
    ExprHandle right = construct(de->right, width_out, etBV);
    return vc_sbvDivExpr(vc, *width_out, left, right);
  }

  case Expr::URem: {
    URemExpr *de = cast<URemExpr>(e);
    ExprHandle left = construct(de->left, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized urem");
    *et_out = etBV;
    
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(de->right)) {
      if (CE->getWidth() <= 64) {
        uint64_t divisor = CE->getZExtValue();

        if (bits64::isPowerOfTwo(divisor)) {
          unsigned bits = bits64::indexOfSingleBit(divisor);

          // special case for modding by 1 or else we bvExtract -1:0
          if (bits == 0) {
            return bvZero(*width_out);
          } else {
            return vc_bvConcatExpr(vc,
                                   bvZero(*width_out - bits),
                                   bvExtract(left, bits - 1, 0));
          }
        }

        // Use fast division to compute modulo without explicit division for
        // constant divisor.

        if (optimizeDivides) {
          if (*width_out == 32) { //only works for 32-bit division
            ExprHandle quotient = constructUDivByConstant( left, *width_out, (uint32_t)divisor );
            ExprHandle quot_times_divisor = constructMulByConstant( quotient, *width_out, divisor );
            ExprHandle rem = vc_bvMinusExpr( vc, *width_out, left, quot_times_divisor );
            return rem;
          }
        }
      }
    }
    
    ExprHandle right = construct(de->right, width_out, etBV);
    return vc_bvModExpr(vc, *width_out, left, right);
  }

  case Expr::SRem: {
    SRemExpr *de = cast<SRemExpr>(e);
    ExprHandle left = construct(de->left, width_out, etBV);
    ExprHandle right = construct(de->right, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized srem");
    *et_out = etBV;

#if 0 //not faster per first benchmark
    if (optimizeDivides) {
      if (ConstantExpr *cre = de->right->asConstant()) {
	uint64_t divisor = cre->asUInt64;

	//use fast division to compute modulo without explicit division for constant divisor
      	if( *width_out == 32 ) { //only works for 32-bit division
	  ExprHandle quotient = constructSDivByConstant( left, *width_out, divisor );
	  ExprHandle quot_times_divisor = constructMulByConstant( quotient, *width_out, divisor );
	  ExprHandle rem = vc_bvMinusExpr( vc, *width_out, left, quot_times_divisor );
	  return rem;
	}
      }
    }
#endif

    // XXX implement my fast path and test for proper handling of sign
    return vc_sbvModExpr(vc, *width_out, left, right);
  }

    // Bitwise

  case Expr::Not: {
    NotExpr *ne = cast<NotExpr>(e);
    ExprHandle expr = construct(ne->expr, width_out, et_out);
    if (*et_out == etBOOL) {
      return vc_notExpr(vc, expr);
    } else {
      return vc_bvNotExpr(vc, expr);
    }
  }    

  case Expr::And: {
    AndExpr *ae = cast<AndExpr>(e);
    ExprHandle left = construct(ae->left, width_out, et_out);
    ExprHandle right = construct(ae->right, width_out, *et_out);
    if (*et_out == etBOOL) {
      return vc_andExpr(vc, left, right);
    } else {
      return vc_bvAndExpr(vc, left, right);
    }
  }

  case Expr::Or: {
    OrExpr *oe = cast<OrExpr>(e);
    ExprHandle left = construct(oe->left, width_out, et_out);
    ExprHandle right = construct(oe->right, width_out, *et_out);
    if (*et_out == etBOOL) {
      return vc_orExpr(vc, left, right);
    } else {
      return vc_bvOrExpr(vc, left, right);
    }
  }

  case Expr::Xor: {
    XorExpr *xe = cast<XorExpr>(e);
    ExprHandle left = construct(xe->left, width_out, et_out);
    ExprHandle right = construct(xe->right, width_out, *et_out);
    
    if (*et_out == etBOOL) {
      // XXX check for most efficient?
      return vc_iteExpr(vc, left, 
                        ExprHandle(vc_notExpr(vc, right)), right);
    } else {
      return vc_bvXorExpr(vc, left, right);
    }
  }

  case Expr::Shl: {
    ShlExpr *se = cast<ShlExpr>(e);
    ExprHandle left = construct(se->left, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized shl");
    *et_out = etBV;

    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(se->right)) {
      return bvLeftShift(left, (unsigned) CE->getLimitedValue(), 
                         getShiftBits(*width_out));
    } else {
      int shiftWidth;
      ExprHandle amount = construct(se->right, &shiftWidth, etBV);
      return bvVarLeftShift( left, amount, *width_out );
    }
  }

  case Expr::LShr: {
    LShrExpr *lse = cast<LShrExpr>(e);
    ExprHandle left = construct(lse->left, width_out, etBV);
    unsigned shiftBits = getShiftBits(*width_out);
    assert(*width_out!=1 && "uncanonicalized lshr");
    *et_out = etBV;

    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(lse->right)) {
      return bvRightShift(left, (unsigned) CE->getLimitedValue(), 
                          shiftBits);
    } else {
      int shiftWidth;
      ExprHandle amount = construct(lse->right, &shiftWidth, etBV);
      return bvVarRightShift( left, amount, *width_out );
    }
  }

  case Expr::AShr: {
    AShrExpr *ase = cast<AShrExpr>(e);
    ExprHandle left = construct(ase->left, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized ashr");
    *et_out = etBV;
    
    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(ase->right)) {
      unsigned shift = (unsigned) CE->getLimitedValue();
      ExprHandle signedBool = bvBoolExtract(left, *width_out-1);
      return constructAShrByConstant(left, shift, signedBool, 
                                     getShiftBits(*width_out));
    } else {
      int shiftWidth;
      ExprHandle amount = construct(ase->right, &shiftWidth, etBV);
      return bvVarArithRightShift( left, amount, *width_out );
    }
  }

    // Comparison

  case Expr::Eq: {
    EqExpr *ee = cast<EqExpr>(e);
    STPExprType src_rt = etDontCare;
    ExprHandle left = construct(ee->left, width_out, &src_rt);
    ExprHandle right = construct(ee->right, width_out, src_rt);
    *et_out = etBOOL;
    *width_out = 1;
    if (src_rt == etBOOL) {
      if (ConstantExpr *CE = dyn_cast<ConstantExpr>(ee->left)) {
        if (CE->isTrue())
          return right;
        return vc_notExpr(vc, right);
      } else {
        return vc_iffExpr(vc, left, right);
      }
    } else {
      return vc_eqExpr(vc, left, right);
    }
  }

  case Expr::Ult: {
    UltExpr *ue = cast<UltExpr>(e);
    ExprHandle left = construct(ue->left, width_out, etBV);
    ExprHandle right = construct(ue->right, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized ult");
    *width_out = 1;
    *et_out = etBOOL;
    return vc_bvLtExpr(vc, left, right);
  }

  case Expr::Ule: {
    UleExpr *ue = cast<UleExpr>(e);
    ExprHandle left = construct(ue->left, width_out, etBV);
    ExprHandle right = construct(ue->right, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized ule");
    *width_out = 1;
    *et_out = etBOOL;
    return vc_bvLeExpr(vc, left, right);
  }

  case Expr::Slt: {
    SltExpr *se = cast<SltExpr>(e);
    ExprHandle left = construct(se->left, width_out, etBV);
    ExprHandle right = construct(se->right, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized slt");
    *width_out = 1;
    *et_out = etBOOL;
    return vc_sbvLtExpr(vc, left, right);
  }

  case Expr::Sle: {
    SleExpr *se = cast<SleExpr>(e);
    ExprHandle left = construct(se->left, width_out, etBV);
    ExprHandle right = construct(se->right, width_out, etBV);
    assert(*width_out!=1 && "uncanonicalized sle");
    *width_out = 1;
    *et_out = etBOOL;
    return vc_sbvLeExpr(vc, left, right);
  }

    // unused due to canonicalization
#if 0
  case Expr::Ne:
  case Expr::Ugt:
  case Expr::Uge:
  case Expr::Sgt:
  case Expr::Sge:
#endif

  case Expr::FPToSI:
  case Expr::FPToUI: {
    std::ostringstream ss;
    ss << "FPtoI" << fpCount++;
    *width_out = e->getWidth();
    *et_out = etBV;
    return buildVar(ss.str().c_str(), e->getWidth());
  }

  default: 
    assert(0 && "unhandled Expr type");
    *et_out = etBOOL;
    return vc_trueExpr(vc);
  }
}
