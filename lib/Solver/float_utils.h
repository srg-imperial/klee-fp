/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com
Ported to KLEE by Peter Collingbourne <peter@pcc.me.uk>

\*******************************************************************/

#ifndef CPROVER_FLOAT_UTILS_H
#define CPROVER_FLOAT_UTILS_H

#include "klee/Expr.h"
#include "llvm/ADT/APSInt.h"

typedef int64_t mp_integer; // APSInt?

class ieee_float_spect
{
public:
  // Bits for fraction (excluding hidden bit) and exponent,
  // respectively
  unsigned f, e;
  
  mp_integer bias() const
  {
    return (1 << (e-1)) - 1;
  }
  
  ieee_float_spect():f(0), e(0)
  {
  }

  ieee_float_spect(unsigned _f, unsigned _e):f(_f), e(_e)
  {
  }

  inline unsigned width() const
  {
    // add one for the sign bit
    return f+e+1;
  }  

  mp_integer max_exponent() const;
  mp_integer max_fraction() const;
  
  // the well-know standard formats  
  inline static ieee_float_spect single_precision()
  {
    return ieee_float_spect(23, 8);
  }

  inline static ieee_float_spect double_precision()
  {
    return ieee_float_spect(52, 11);
  }
};

class ieee_floatt
{
public:
  // ROUND_TO_EVEN is also known as "round to nearest, ties to even", and
  // is the IEEE default
  typedef enum {
    ROUND_TO_EVEN, ROUND_TO_ZERO, ROUND_TO_PLUS_INF, ROUND_TO_MINUS_INF,
    UNKNOWN, NONDETERMINISTIC }
    rounding_modet;
};

typedef klee::ref<klee::Expr> bvt, literalt;

class propt
{
public:
  typedef enum { LEFT, LRIGHT, ARIGHT } shiftt;

  bvt absolute_value(const bvt &bv) {
    return klee::SelectExpr::create(sign_bit(bv),
      klee::SubExpr::create(klee::ConstantExpr::create(0, bv->getWidth()), bv),
      bv);
  }

  literalt bit(const bvt &src, unsigned b) {
    return klee::ExtractExpr::create(src, b, klee::Expr::Bool);
  }

  literalt sign_bit(const bvt &src) {
    // this is the top bit
    return bit(src, width(src)-1);
  }

  unsigned width(const bvt &bv) {
    return bv->getWidth();
  }

  bvt build_constant(const mp_integer &i, unsigned width) {
    return klee::ConstantExpr::create(i, width);
  }

  bvt extract(const bvt &a, unsigned first, unsigned last) const {
    return klee::ExtractExpr::create(a, first, last-first);
  }

  literalt land(literalt a, literalt b) {
    return klee::AndExpr::create(a, b);
  }

  literalt lnot(literalt a) {
    return klee::NotExpr::create(a);
  }

  bvt sign_extension(const bvt &bv, unsigned new_size) {
    return klee::SExtExpr::create(bv, new_size);
  }

  bvt zero_extension(const bvt &bv, unsigned new_size) {
    return klee::ZExtExpr::create(bv, new_size);
  }

  bvt add(const bvt &op0, const bvt &op1) {
    return klee::AddExpr::create(op0, op1);
  }

  bvt sub(const bvt &op0, const bvt &op1) {
    return klee::SubExpr::create(op0, op1);
  }

  bvt shift(const bvt &op, const shiftt shift, unsigned distance) {
    return this->shift(op, shift, build_constant(distance, width(op)));
  }

  bvt shift(const bvt &op, const shiftt shift, const bvt &distance) {
    switch (shift) {
    case LEFT:   return klee::ShlExpr::create(op, distance);
    case LRIGHT: return klee::LShrExpr::create(op, distance);
    case ARIGHT: return klee::AShrExpr::create(op, distance);
    }
  }
};

typedef propt bv_utilst;

literalt const_literal(bool b)
{
  return klee::ConstantExpr::create(b, klee::Expr::Bool);
}

class float_utilst
{
public:
  ieee_floatt::rounding_modet rounding_mode;

  struct rounding_mode_bitst
  {
  public:
    // these are mutually exclusive, obviously
    literalt round_to_even;
    literalt round_to_zero;
    literalt round_to_plus_inf;
    literalt round_to_minus_inf;

    rounding_mode_bitst():
      round_to_even(const_literal(true)),
      round_to_zero(const_literal(false)),
      round_to_plus_inf(const_literal(false)),
      round_to_minus_inf(const_literal(false))
    {
    }
  };

  float_utilst(propt &_prop):
    rounding_mode(ieee_floatt::ROUND_TO_EVEN),
    prop(_prop),
    bv_utils(_prop)
  {
  }

  virtual ~float_utilst()
  {
  }

  ieee_float_spect spec;

  bvt build_constant(const ieee_floatt &src);

  literalt sign_bit(const bvt &src) { return bv_utils.sign_bit(src); }

  // extraction
  bvt get_exponent(const bvt &src); // still biased
  bvt get_fraction(const bvt &src); // without hidden bit
  literalt is_normal(const bvt &src);
  literalt is_zero(const bvt &src); // this returns true on both 0 and -0
  literalt is_infinity(const bvt &src);
  literalt is_plus_inf(const bvt &src);
  literalt is_minus_inf(const bvt &src);
  literalt is_NaN(const bvt &src);

  // add/sub
  virtual bvt add_sub(const bvt &src1, const bvt &src2, bool subtract);
  bvt add(const bvt &src1, const bvt &src2) { return add_sub(src1, src2, false); }
  bvt sub(const bvt &src1, const bvt &src2) { return add_sub(src1, src2, true); }

  // mul/div
  virtual bvt mul(const bvt &src1, const bvt &src2);
  virtual bvt div(const bvt &src1, const bvt &src2);

  bvt abs(const bvt &src);
  bvt inverse(const bvt &src);
  bvt negate(const bvt &src);

  // conversion
  bvt from_unsigned_integer(const bvt &src);
  bvt from_signed_integer(const bvt &src);
  bvt to_integer(const bvt &src, unsigned int_width, bool is_signed);
  bvt to_signed_integer(const bvt &src, unsigned int_width);
  bvt to_unsigned_integer(const bvt &src, unsigned int_width);
  bvt conversion(const bvt &src, const ieee_float_spect &dest_spec);

  // relations
  typedef enum { LT, LE, EQ, GT, GE } relt;
  literalt relation(const bvt &src1, relt rel, const bvt &src2);

  // constants
  ieee_floatt get(const bvt &src) const;

  // helpers
  literalt exponent_all_ones(const bvt &src);
  literalt exponent_all_zeros(const bvt &src);
  literalt fraction_all_zeros(const bvt &src);
    
  // debugging hooks
  bvt debug1(const bvt &op0, const bvt &op1);
  bvt debug2(const bvt &op0, const bvt &op1);

protected:
  propt &prop;
  bv_utilst bv_utils;

  // unpacked
  virtual void normalization_shift(bvt &fraction, bvt &exponent);
  void denormalization_shift(bvt &fraction, bvt &exponent);

  bvt add_bias(const bvt &exponent);
  bvt sub_bias(const bvt &exponent);

  bvt limit_distance(const bvt &dist, mp_integer limit);

  struct unpacked_floatt
  {
    literalt sign;
    literalt infinity;
    literalt zero;
    literalt NaN;
    bvt fraction;
    bvt exponent;

    unpacked_floatt():
      sign(const_literal(false)),
      infinity(const_literal(false)),
      zero(const_literal(false)),
      NaN(const_literal(false))
    {
    }
  };

  // this has a biased exponent
  // and an _implicit_ hidden bit
  struct biased_floatt:public unpacked_floatt
  {
  };

  // the hidden bit is explicit,
  // and the exponent is not biased
  struct unbiased_floatt:public unpacked_floatt
  {
  };

  biased_floatt bias(const unbiased_floatt &src);

  // this takes unpacked format, and returns packed
  virtual bvt rounder(const unbiased_floatt &src);
  bvt pack(const biased_floatt &src);
  unbiased_floatt unpack(const bvt &src);

  void round_fraction(unbiased_floatt &result);
  void round_exponent(unbiased_floatt &result);

  // helpers for adder

  // computes src1.exponent-src2.exponent with extension
  bvt subtract_exponents(
    const unbiased_floatt &src1,
    const unbiased_floatt &src2);

  // computes the "sticky-bit"
  bvt sticky_right_shift(
    const bvt &op,
    const bv_utilst::shiftt shift_type,
    const bvt &dist,
    literalt &sticky);
};

#endif
