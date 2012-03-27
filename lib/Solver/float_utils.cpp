/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com
Ported to KLEE by Peter Collingbourne <peter@pcc.me.uk>

(C) 2001-2011, Daniel Kroening, Edmund Clarke,
Computer Science Department, Oxford University
Computer Systems Institute, ETH Zurich
Computer Science Department, Carnegie Mellon University

All rights reserved. Redistribution and use in source and binary forms, with
or without modification, are permitted provided that the following
conditions are met:

  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.

  3. All advertising materials mentioning features or use of this software
     must display the following acknowledgement:

     This product includes software developed by Daniel Kroening,
     Edmund Clarke,
     Computer Science Department, Oxford University
     Computer Systems Institute, ETH Zurich
     Computer Science Department, Carnegie Mellon University

  4. Neither the name of the University nor the names of its contributors
     may be used to endorse or promote products derived from this software
     without specific prior written permission.

   
THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS `AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

\*******************************************************************/

#include <cassert>

#include "float_utils.h"

static mp_integer address_bits(const mp_integer &size)
{
  mp_integer result, x=2;

  for(result=1; x<size; result+=1, x*=2);

  return result;
}

/*******************************************************************\

Function: float_utilst::from_signed_integer

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::from_signed_integer(const bvt &src)
{
  unbiased_floatt result;

  // we need to convert negative integers
  result.sign=sign_bit(src);

  result.fraction=bv_utils.absolute_value(src);

  // build an exponent (unbiased) -- this is signed!
  result.exponent=
    bv_utils.build_constant(
      bv_utils.width(src)-1,
      address_bits(bv_utils.width(src)-1)+1);

  return rounder(result);
}

/*******************************************************************\

Function: float_utilst::from_unsigned_integer

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::from_unsigned_integer(const bvt &src)
{
  unbiased_floatt result;

  result.fraction=src;

  // build an exponent (unbiased) -- this is signed!
  result.exponent=
    bv_utils.build_constant(
      bv_utils.width(src)-1,
      address_bits(bv_utils.width(src)-1)+1);

  result.sign=const_literal(false);

  return rounder(result);
}

/*******************************************************************\

Function: float_utilst::to_signed_integer

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::to_signed_integer(const bvt &src, unsigned dest_width)
{
  return to_integer(src, dest_width, true);
}

/*******************************************************************\

Function: float_utilst::to_unsigned_integer

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::to_unsigned_integer(const bvt &src, unsigned dest_width)
{
  return to_integer(src, dest_width, false);
}

/*******************************************************************\

Function: float_utilst::to_integer

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::to_integer(
  const bvt &src,
  unsigned dest_width,
  bool is_signed)
{
  assert(bv_utils.width(src)==spec.width());

  unbiased_floatt unpacked=unpack(src);

  // for the other rounding modes, we need a correct exponent, though we may
  // be able to get away with a truncated exponent in some cases
  if (rounding_mode != ieee_floatt::ROUND_TO_ZERO)
    unpacked.exponent=bv_utils.sign_extension(unpacked.exponent,
                                              bv_utils.width(unpacked.exponent)+1);

  // if the exponent is positive, shift right
  bvt offset=bv_utils.build_constant(spec.f, bv_utils.width(unpacked.exponent));
  bvt distance=bv_utils.sub(offset, unpacked.exponent);
  bvt result;

  if (rounding_mode == ieee_floatt::ROUND_TO_ZERO) {
    result=bv_utils.shift(unpacked.fraction, bv_utilst::LRIGHT, distance);

    // if the exponent is negative, we have zero anyways
    literalt exponent_sign=sign_bit(unpacked.exponent);
    result=prop.land(result, bv_utils.sign_extension(prop.lnot(exponent_sign),
                                                     bv_utils.width(result)));
  } else {
    unsigned result_width=bv_utils.width(unpacked.fraction);

    // 2^(spec.e + 1) bits ought to be enough for anyone
    // TODO: figure out the maximum number of extra bits we need here
    unsigned round_width=2<<spec.e;
    unsigned extra_bits=round_width-result_width;
    result=bv_utils.concatenate(bv_utils.zeros(extra_bits),
                                unpacked.fraction);
    result=bv_utils.shift(result, bv_utilst::LRIGHT, distance);

    literalt increment=need_increment(result_width, unpacked.sign, result);

    // chop off all the extra bits
    result=bv_utils.extract(result, extra_bits, round_width-1);

    result=bv_utils.incrementer(result, increment);
  }

  // chop out the right number of bits from the result
  if(bv_utils.width(result)>dest_width)
  {
    result=bv_utils.extract(result, 0, dest_width-1);
  }
  else if(bv_utils.width(result)<dest_width)
  {
    // extend
    result=bv_utils.zero_extension(result, dest_width);
  }

  assert(bv_utils.width(result)==dest_width);

  if(is_signed)
    result=bv_utils.cond_negate(result, unpacked.sign);

  return result;
}

/*******************************************************************\

Function: float_utilst::conversion

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::conversion(
  const bvt &src,
  const ieee_float_spect &dest_spec)
{
  assert(bv_utils.width(src)==spec.width());

  // catch the special case in which we extend,
  // e.g., single to double
  
  if(dest_spec.e>=spec.e &&
     dest_spec.f>=spec.f)
  {
    unbiased_floatt unpacked_src=unpack(src);
    unbiased_floatt result;
    
    // the fraction gets zero-padded
    unsigned padding=dest_spec.f-spec.f;
    result.fraction=
      bv_utils.concatenate(bv_utils.zeros(padding), unpacked_src.fraction);
    
    // the exponent gets sign-extended
    result.exponent=
      bv_utils.sign_extension(unpacked_src.exponent, dest_spec.e);

    // the flags get copied
    result.sign=unpacked_src.sign;
    result.NaN=unpacked_src.NaN;
    result.infinity=unpacked_src.infinity;

    // no rounding needed!
    spec=dest_spec;
    return pack(bias(result));
  }
  else
  {
    // we actually need to round
    unbiased_floatt result=unpack(src);
    spec=dest_spec;
    return rounder(result);
  }
}

/*******************************************************************\

Function: float_utilst::is_normal

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt float_utilst::is_normal(const bvt &src)
{
  return prop.land(
           prop.lnot(exponent_all_zeros(src)),
           prop.lnot(exponent_all_ones(src)));
}

/*******************************************************************\

Function: float_utilst::subtract_exponents

  Inputs:

 Outputs:

 Purpose: Subtracts the exponents

\*******************************************************************/

bvt float_utilst::subtract_exponents(
  const unbiased_floatt &src1,
  const unbiased_floatt &src2)
{
  // extend both
  bvt extended_exponent1=bv_utils.sign_extension(src1.exponent, bv_utils.width(src1.exponent)+1);
  bvt extended_exponent2=bv_utils.sign_extension(src2.exponent, bv_utils.width(src2.exponent)+1);

  assert(bv_utils.width(extended_exponent1)==bv_utils.width(extended_exponent2));

  // compute shift distance (here is the subtraction)
  return bv_utils.sub(extended_exponent1, extended_exponent2);
}

/*******************************************************************\

Function: float_utilst::add_sub

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::add_sub(
  const bvt &src1,
  const bvt &src2,
  bool subtract)
{
  unbiased_floatt unpacked1=unpack(src1);
  unbiased_floatt unpacked2=unpack(src2);

  // subtract?
  if(subtract)
    unpacked2.sign=prop.lnot(unpacked2.sign);

  // figure out which operand has the bigger exponent
  const bvt exponent_difference=subtract_exponents(unpacked1, unpacked2);
  literalt src2_bigger=bv_utils.sign_bit(exponent_difference);

  const bvt bigger_exponent=
    bv_utils.select(src2_bigger, unpacked2.exponent, unpacked1.exponent);

  // swap fractions as needed
  const bvt new_fraction1=
    bv_utils.select(src2_bigger, unpacked2.fraction, unpacked1.fraction);

  const bvt new_fraction2=
    bv_utils.select(src2_bigger, unpacked1.fraction, unpacked2.fraction);

  // compute distance
  const bvt distance=bv_utils.absolute_value(exponent_difference);

  // limit the distance: shifting more than f+3 bits is unnecessary
  const bvt limited_dist=limit_distance(distance, spec.f+3);

  // pad fractions with 2 zeros from below
  const bvt fraction1_padded=bv_utils.concatenate(bv_utils.zeros(3), new_fraction1);
  const bvt fraction2_padded=bv_utils.concatenate(bv_utils.zeros(3), new_fraction2);

  // shift new_fraction2
  literalt sticky_bit;
  const bvt fraction1_shifted=fraction1_padded;
  const bvt fraction2_shifted=sticky_right_shift(
    fraction2_padded, bv_utilst::LRIGHT, limited_dist, sticky_bit);

  // sticky bit: or of the bits lost by the right-shift
  bvt fraction2_stickied=bv_utils.concatenate(
      prop.lor(bv_utils.bit(fraction2_shifted, 0), sticky_bit),
      bv_utils.extract(fraction2_shifted, 1, bv_utils.width(fraction2_shifted)-1));

  // need to have two extra fraction bits for addition and rounding
  const bvt fraction1_ext=bv_utils.zero_extension(fraction1_shifted, bv_utils.width(fraction1_shifted)+2);
  const bvt fraction2_ext=bv_utils.zero_extension(fraction2_stickied, bv_utils.width(fraction2_stickied)+2);

  unbiased_floatt result;

  // now add/sub them
  literalt subtract_lit=prop.lxor(unpacked1.sign, unpacked2.sign);
  result.fraction=
    bv_utils.add_sub(fraction1_ext, fraction2_ext, subtract_lit);

  // sign of result
  literalt fraction_sign=sign_bit(result.fraction);
  result.fraction=bv_utils.absolute_value(result.fraction);

  result.exponent=bigger_exponent;

  // adjust the exponent for the fact that we added two bits to the fraction
  result.exponent=
    bv_utils.add(bv_utils.sign_extension(result.exponent, bv_utils.width(result.exponent)+1),
      bv_utils.build_constant(2, bv_utils.width(result.exponent)+1));

  // NaN?
  result.NaN=prop.lor(
      prop.land(prop.land(unpacked1.infinity, unpacked2.infinity),
                prop.lxor(unpacked1.sign, unpacked2.sign)),
      prop.lor(unpacked1.NaN, unpacked2.NaN));

  // infinity?
  result.infinity=prop.land(
      prop.lnot(result.NaN),
      prop.lor(unpacked1.infinity, unpacked2.infinity));

  // sign
  literalt add_sub_sign=
    prop.lxor(prop.lselect(src2_bigger, unpacked2.sign, unpacked1.sign),
              fraction_sign);

  literalt infinity_sign=
    prop.lselect(unpacked1.infinity, unpacked1.sign, unpacked2.sign);

  result.sign=prop.lselect(
    result.infinity,
    infinity_sign,
    add_sub_sign);

  #if 0
  result.sign=const_literal(false);
  result.fraction.resize(spec.f+1, const_literal(true));
  result.exponent.resize(spec.e, const_literal(false));
  result.NaN=const_literal(false);
  result.infinity=const_literal(false);
  //for(unsigned i=0; i<result.fraction.size(); i++)
  //  result.fraction[i]=const_literal(true);

  for(unsigned i=0; i<result.fraction.size(); i++)
    result.fraction[i]=new_fraction2[i];

  return pack(bias(result));
  #endif

  return rounder(result);
}

/*******************************************************************\

Function: float_utilst::limit_distance

  Inputs:

 Outputs:

 Purpose: Limits the shift distance

\*******************************************************************/

bvt float_utilst::limit_distance(
    const bvt &dist,
    mp_integer limit)
{
  int nb_bits=address_bits(limit);

  bvt upper_bits=bv_utils.extract(dist, nb_bits, bv_utils.width(dist)-1);
  literalt or_upper_bits=prop.lor(upper_bits);

  bvt lower_bits=bv_utils.extract(dist, 0, nb_bits-1);

  bvt result=prop.lor(lower_bits,
                      bv_utils.sign_extension(or_upper_bits, nb_bits));
  return result;
}

/*******************************************************************\

Function: float_utilst::mul

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::mul(const bvt &src1, const bvt &src2)
{
  // unpack
  const unbiased_floatt unpacked1=unpack(src1);
  const unbiased_floatt unpacked2=unpack(src2);

  // zero-extend the fractions
  const bvt fraction1=bv_utils.zero_extension(unpacked1.fraction, bv_utils.width(unpacked1.fraction)*2);
  const bvt fraction2=bv_utils.zero_extension(unpacked2.fraction, bv_utils.width(unpacked2.fraction)*2);

  // multiply fractions
  unbiased_floatt result;
  result.fraction=bv_utils.unsigned_multiplier(fraction1, fraction2);

  // extend exponents to account for overflow
  // add two bits, as we do extra arithmetic on it later
  const bvt exponent1=bv_utils.sign_extension(unpacked1.exponent, bv_utils.width(unpacked1.exponent)+2);
  const bvt exponent2=bv_utils.sign_extension(unpacked2.exponent, bv_utils.width(unpacked2.exponent)+2);

  bvt added_exponent=bv_utils.add(exponent1, exponent2);

  // adjust, we are thowing in an extra fraction bit
  // it has been extended above
  result.exponent=bv_utils.inc(added_exponent);

  // new sign
  result.sign=prop.lxor(unpacked1.sign, unpacked2.sign);

  // infinity?
  result.infinity=prop.lor(unpacked1.infinity, unpacked2.infinity);

  // NaN?
  {
    std::vector<literalt> NaN_cond;

    NaN_cond.push_back(is_NaN(src1));
    NaN_cond.push_back(is_NaN(src2));

    // infinity * 0 is NaN!
    NaN_cond.push_back(prop.land(is_zero(src1), unpacked2.infinity));
    NaN_cond.push_back(prop.land(is_zero(src2), unpacked1.infinity));

    result.NaN=prop.lor(NaN_cond);
  }

  return rounder(result);
}

/*******************************************************************\

Function: float_utilst::div

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::div(const bvt &src1, const bvt &src2)
{
  // unpack
  const unbiased_floatt unpacked1=unpack(src1);
  const unbiased_floatt unpacked2=unpack(src2);
  
  unsigned div_width=bv_utils.width(unpacked1.fraction)*2+1;

  // pad fraction1 with zeros
  bvt fraction1=bv_utils.concatenate(bv_utils.zeros(div_width-bv_utils.width(unpacked1.fraction)),
                                     unpacked1.fraction);

  // zero-extend fraction2
  const bvt fraction2=
    bv_utils.zero_extension(unpacked2.fraction, div_width);

  // divide fractions
  unbiased_floatt result;
  bvt rem;
  bv_utils.unsigned_divider(fraction1, fraction2, result.fraction, rem);
  
  // is there a remainder?
  literalt have_remainder=bv_utils.is_not_zero(rem);
  
  // we throw this into the result, as one additional bit,
  // to get the right rounding decision
  result.fraction=bv_utils.concatenate(have_remainder, result.fraction);
    
  // We will subtract the exponents;
  // to account for overflow, we add a bit.
  const bvt exponent1=bv_utils.sign_extension(unpacked1.exponent, bv_utils.width(unpacked1.exponent)+1);
  const bvt exponent2=bv_utils.sign_extension(unpacked2.exponent, bv_utils.width(unpacked2.exponent)+1);

  // subtract exponents
  bvt added_exponent=bv_utils.sub(exponent1, exponent2);

  // adjust, as we have thown in extra fraction bits
  result.exponent=bv_utils.add(
    added_exponent,
    bv_utils.build_constant(spec.f, bv_utils.width(added_exponent)));

  // new sign
  result.sign=prop.lxor(unpacked1.sign, unpacked2.sign);

  // infinity?
  // in particular, inf/0=inf
  result.infinity=prop.land(prop.lnot(unpacked1.zero),
                  prop.land(prop.lnot(unpacked1.NaN),
             	            unpacked2.zero));

  // NaN?
  result.NaN=prop.lor(unpacked1.NaN,
             prop.lor(unpacked2.NaN,
             prop.lor(prop.land(unpacked1.zero, unpacked2.zero),
                      prop.land(unpacked1.infinity, unpacked2.infinity))));

  return rounder(result);
}

/*******************************************************************\

Function: float_utilst::inverse

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::inverse(const bvt &src)
{
  bvt one;
  assert(0);
  return bvt(); // not reached
}

/*******************************************************************\

Function: float_utilst::negate

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::negate(const bvt &src)
{
  assert(bv_utils.width(src)!=0);
  return bv_utils.concatenate(bv_utils.extract(src, 0, bv_utils.width(src)-2),
                              prop.lnot(sign_bit(src)));
}

/*******************************************************************\

Function: float_utilst::abs

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::abs(const bvt &src)
{
  assert(bv_utils.width(src)!=0);
  return bv_utils.concatenate(bv_utils.extract(src, 0, bv_utils.width(src)-2),
                              const_literal(false));
}

/*******************************************************************\

Function: float_utilst::relation

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt float_utilst::relation(
  const bvt &src1,
  relt rel,
  const bvt &src2)
{
  if(rel==GT)
    return relation(src2, LT, src1); // swapped
  else if(rel==GE)
    return relation(src2, LE, src1); // swapped

  assert(rel==EQ || rel==LT || rel==LE);

  // special cases: -0 and 0 are equal
  literalt is_zero1=is_zero(src1);
  literalt is_zero2=is_zero(src2);
  literalt both_zero=prop.land(is_zero1, is_zero2);

  // NaN compares to nothing
  literalt is_NaN1=is_NaN(src1);
  literalt is_NaN2=is_NaN(src2);
  literalt NaN=prop.lor(is_NaN1, is_NaN2);

  if(rel==LT || rel==LE)
  {
    literalt bitwise_equal=bv_utils.equal(src1, src2);

    // signs different? trivial! Unless Zero.

    literalt signs_different=
      prop.lxor(sign_bit(src1), sign_bit(src2));

    // as long as the signs match: compare like unsigned numbers

    // this works due to the BIAS
    literalt less_than1=bv_utils.unsigned_less_than(src1, src2);

    // if both are negative (and not the same), need to turn around!
    literalt less_than2=
        prop.lxor(less_than1, prop.land(sign_bit(src1), sign_bit(src2)));

    literalt less_than3=
      prop.lselect(signs_different,
        sign_bit(src1),
        less_than2);

    if(rel==LT)
    {
      std::vector<literalt> and_bv;
      and_bv.push_back(less_than3);
      and_bv.push_back(prop.lnot(bitwise_equal)); // for the case of two negative numbers
      and_bv.push_back(prop.lnot(both_zero));
      and_bv.push_back(prop.lnot(NaN));

      return prop.land(and_bv);
    }
    else if(rel==LE)
    {
      std::vector<literalt> or_bv;
      or_bv.push_back(less_than3);
      or_bv.push_back(both_zero);
      or_bv.push_back(bitwise_equal);

      return prop.land(prop.lor(or_bv), prop.lnot(NaN));
    }
    else
      assert(false);
  }
  else if(rel==EQ)
  {
    literalt bitwise_equal=bv_utils.equal(src1, src2);

    return prop.land(
      prop.lor(bitwise_equal, both_zero),
      prop.lnot(NaN));
  }
  else
    assert(0);
    
  // not reached
  return const_literal(false);
}

/*******************************************************************\

Function: float_utilst::is_zero

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt float_utilst::is_zero(const bvt &src)
{
  assert(bv_utils.width(src)!=0);
  return bv_utils.is_zero(bv_utils.extract(src, 0, bv_utils.width(src)-2));
}

/*******************************************************************\

Function: float_utilst::is_plus_inf

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt float_utilst::is_plus_inf(const bvt &src)
{
  std::vector<literalt> and_bv;
  and_bv.push_back(prop.lnot(sign_bit(src)));
  and_bv.push_back(exponent_all_ones(src));
  and_bv.push_back(fraction_all_zeros(src));
  return prop.land(and_bv);
}

/*******************************************************************\

Function: float_utilst::infinity

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt float_utilst::is_infinity(const bvt &src)
{
  return prop.land(
    exponent_all_ones(src),
    fraction_all_zeros(src));
}

/*******************************************************************\

Function: float_utilst::get_exponent

  Inputs:

 Outputs:

 Purpose: Gets the unbiased exponent in a floating-point bit-vector

\*******************************************************************/

bvt float_utilst::get_exponent(const bvt &src)
{
  return bv_utils.extract(src, spec.f, spec.f+spec.e-1);
}

/*******************************************************************\

Function: float_utilst::get_fraction

  Inputs:

 Outputs:

 Purpose: Gets the fraction without hidden bit in a floating-point
          bit-vector src

\*******************************************************************/

bvt float_utilst::get_fraction(const bvt &src)
{
  return bv_utils.extract(src, 0, spec.f-1);
}

/*******************************************************************\

Function: float_utilst::is_minus_inf

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt float_utilst::is_minus_inf(const bvt &src)
{
  std::vector<literalt> and_bv;
  and_bv.push_back(sign_bit(src));
  and_bv.push_back(exponent_all_ones(src));
  and_bv.push_back(fraction_all_zeros(src));
  return prop.land(and_bv);
}

/*******************************************************************\

Function: float_utilst::is_NaN

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt float_utilst::is_NaN(const bvt &src)
{
  return prop.land(exponent_all_ones(src),
                   prop.lnot(fraction_all_zeros(src)));
}

/*******************************************************************\

Function: float_utilst::exponent_all_ones

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt float_utilst::exponent_all_ones(const bvt &src)
{
  bvt exponent=bv_utils.extract(src, spec.f, spec.f+spec.e-1);
  return bv_utils.is_all_ones(exponent);
}

/*******************************************************************\

Function: float_utilst::exponent_all_zeros

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt float_utilst::exponent_all_zeros(const bvt &src)
{
  bvt exponent=bv_utils.extract(src, spec.f, spec.f+spec.e-1);
  return bv_utils.is_zero(exponent);
}

/*******************************************************************\

Function: float_utilst::fraction_all_zeros

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

literalt float_utilst::fraction_all_zeros(const bvt &src)
{
  // does not include hidden bit
  bvt tmp=bv_utils.extract(src, 0, spec.f-1);
  return bv_utils.is_zero(tmp);
}

/*******************************************************************\

Function: float_utilst::normalization_shift

  Inputs:

 Outputs:

 Purpose: normalize fraction/exponent pair

\*******************************************************************/

void float_utilst::normalization_shift(bvt &fraction, bvt &exponent)
{
  // this thing is quadratic!
  // TODO: replace by n log n construction
  // returns 'zero' if fraction is zero

  // fraction all zero? it stays zero
  // the exponent is undefined in that case
  bvt new_fraction=bv_utils.zeros(bv_utils.width(fraction));
  bvt new_exponent=bv_utils.zeros(bv_utils.width(exponent)); // TODO: make undefined

  // i is the shift distance
  for(unsigned i=0; i<bv_utils.width(fraction); i++)
  {
    std::vector<literalt> equal;

    // the bits above need to be zero
    for(unsigned j=0; j<i; j++)
      equal.push_back(
        prop.lnot(bv_utils.bit(fraction, bv_utils.width(fraction)-1-j)));

    // this one needs to be one
    equal.push_back(bv_utils.bit(fraction, bv_utils.width(fraction)-1-i));

    // iff all of that holds, we shift here!
    literalt shift=prop.land(equal);

    // build shifted value
    bvt shifted_fraction=bv_utils.shift(fraction, bv_utilst::LEFT, i);
    new_fraction=bv_utils.select(shift, shifted_fraction, new_fraction);

    // build new exponent
    bvt adjustment=bv_utils.build_constant(-i, bv_utils.width(exponent));
    bvt added_exponent=bv_utils.add(exponent, adjustment);
    new_exponent=bv_utils.select(shift, added_exponent, new_exponent);
  }

  fraction=new_fraction;
  exponent=new_exponent;
}

/*******************************************************************\

Function: float_utilst::denormalization_shift

  Inputs:

 Outputs:

 Purpose: make sure exponent is not too small;
          the exponent is unbiased

\*******************************************************************/

void float_utilst::denormalization_shift(bvt &fraction, bvt &exponent)
{
  mp_integer bias=spec.bias();

  // is the exponent strictly less than -bias+1, i.e., exponent<-bias+1?
  // this is transformed to distance=(-bias+1)-exponent
  // i.e., distance>0

  assert(bv_utils.width(exponent)>=spec.e);

  bvt distance=bv_utils.sub(
    bv_utils.build_constant(-bias+1, bv_utils.width(exponent)), exponent);

  // use sign bit
  literalt denormal=prop.land(
    prop.lnot(sign_bit(distance)),
    prop.lnot(bv_utils.is_zero(distance)));

  #if 1
  fraction=
    bv_utils.select(
      denormal,
      bv_utils.shift(fraction, bv_utilst::LRIGHT, distance),
      fraction);

  exponent=
    bv_utils.select(denormal,
      bv_utils.build_constant(-bias, bv_utils.width(exponent)),
      exponent);

  #else
  //for(unsigned i=0; i<fraction.size(); i++)
  //  fraction[i]=const_literal(0);

  bvt bvbias=bv_utils.build_constant(bias, exponent.size());
  bvt expadd=bv_utils.add(exponent, bvbias);

  for(unsigned i=0; i<exponent.size(); i++)
    fraction[i+2]=exponent[i];
  //for(unsigned i=0; i<exponent.size(); i++)
  //  fraction[i+2+exponent.size()]=distance[i];
  #endif
}

/*******************************************************************\

Function: float_utilst::rounder

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::rounder(const unbiased_floatt &src)
{
  // incoming: some fraction (with explicit 1),
  // some exponent without bias
  // outgoing: rounded, with right size, with hidden bit, bias

  bvt aligned_fraction=src.fraction,
      aligned_exponent=src.exponent;

  {
    unsigned exponent_bits=
      std::max((unsigned long)address_bits(spec.f),
               (unsigned long)spec.e)+1;

    // before normalization, make sure exponent is large enough
    if(bv_utils.width(aligned_exponent)<exponent_bits)
    {
      // sign extend
      aligned_exponent=
        bv_utils.sign_extension(aligned_exponent, exponent_bits);
    }
  }

  // align it!
  normalization_shift(aligned_fraction, aligned_exponent);
  denormalization_shift(aligned_fraction, aligned_exponent);

  unbiased_floatt result;
  result.fraction=aligned_fraction;
  result.exponent=aligned_exponent;
  result.sign=src.sign;
  result.NaN=src.NaN;
  result.infinity=src.infinity;

  round_fraction(result);
  round_exponent(result);

  return pack(bias(result));
}

/*******************************************************************\

Function: float_utilst::need_increment

  Inputs:

 Outputs:

 Purpose: Determines whether the given fraction must be incremented
          under the current rounding mode after being truncated to
          the given number of bits.

\*******************************************************************/

literalt float_utilst::need_increment(unsigned bits, literalt sign,
                                      const bvt &fraction)
{
  unsigned extra_bits=bv_utils.width(fraction)-bits;

  // more than two extra bits are superfluous, and are
  // turned into a sticky bit

  literalt sticky_bit=const_literal(false);

  if(extra_bits>=2)
  {
    bvt tail=bv_utils.extract(fraction, 0, extra_bits-2);
    sticky_bit=prop.lor(tail);
  }

  // the rounding bit is the last extra bit
  assert(extra_bits>=1);
  literalt rounding_bit=bv_utils.bit(fraction, extra_bits-1);

  literalt rounding_least=bv_utils.bit(fraction, extra_bits);
  literalt increment;

  switch(rounding_mode)
  {
  case ieee_floatt::ROUND_TO_EVEN: // round-to-nearest (ties to even)
    increment=prop.land(rounding_bit,
                        prop.lor(rounding_least, sticky_bit));
    break;

  case ieee_floatt::ROUND_TO_PLUS_INF: // round up
    increment=prop.land(prop.lnot(sign),
                        prop.lor(rounding_bit, sticky_bit));
    break;

  case ieee_floatt::ROUND_TO_MINUS_INF: // round down
    increment=prop.land(sign,
                        prop.lor(rounding_bit, sticky_bit));
    break;

  case ieee_floatt::ROUND_TO_ZERO: // round to zero
    increment=const_literal(false);
    break;

  case ieee_floatt::UNKNOWN: // UNKNOWN, rounding is not determined
    assert(0);
    break;
    
  case ieee_floatt::NONDETERMINISTIC:
    // the increment is non-deterministic
    // increment=prop.new_variable(); // TODO: make this non-deterministic.
    increment=const_literal(false);
    break;
  
  default:
    assert(0);
  }

  return increment;
}

/*******************************************************************\

Function: float_utilst::round_fraction

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void float_utilst::round_fraction(unbiased_floatt &result)
{
  unsigned fraction_size=spec.f+1;

  // do we need to enlarge the fraction?
  if(bv_utils.width(result.fraction)<fraction_size)
  {
    // pad with zeros at bottom
    unsigned padding=fraction_size-bv_utils.width(result.fraction);

    result.fraction=bv_utils.concatenate(
      bv_utils.zeros(padding),
      result.fraction);

    assert(bv_utils.width(result.fraction)==fraction_size);
  }
  else if(bv_utils.width(result.fraction)==fraction_size) // it stays
  {
    // do nothing
  }
  else // fraction gets smaller -- rounding
  {
    unsigned extra_bits=bv_utils.width(result.fraction)-fraction_size;

    // determine if we need increment
    literalt increment=need_increment(fraction_size, result.sign, result.fraction);

    // chop off all the extra bits
    result.fraction=bv_utils.extract(
      result.fraction, extra_bits, bv_utils.width(result.fraction)-1);

    assert(bv_utils.width(result.fraction)==fraction_size);

    // incrementing the fraction might result in an overflow
    result.fraction=
      bv_utils.zero_extension(result.fraction, bv_utils.width(result.fraction)+1);

    result.fraction=bv_utils.incrementer(result.fraction, increment);

    literalt overflow=sign_bit(result.fraction);

    // in case of an overflow, the exponent has to be incremented
    // post normalization is then required
    result.exponent=
      bv_utils.incrementer(result.exponent, overflow);

    // post normalization of the fraction
    literalt integer_part1=sign_bit(result.fraction);
    literalt integer_part0=bv_utils.bit(result.fraction, bv_utils.width(result.fraction)-2);
    literalt new_integer_part=prop.lor(integer_part1, integer_part0);

    result.fraction=bv_utils.concatenate(
        bv_utils.extract(result.fraction, 0, bv_utils.width(result.fraction)-3),
        new_integer_part);
  }
}

/*******************************************************************\

Function: float_utilst::round_exponent

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void float_utilst::round_exponent(unbiased_floatt &result)
{
  // do we need to enlarge the exponent?
  if(bv_utils.width(result.exponent)<spec.e)
  {
    // should have been done before
    assert(false);
  }
  else if(bv_utils.width(result.exponent)==spec.e) // it stays
  {
    // do nothing
  }
  else // exponent gets smaller -- chop off top bits
  {
    bvt old_exponent=result.exponent;
    result.exponent=bv_utils.extract(result.exponent, 0, spec.e-1);

    bvt max_exponent=
      bv_utils.build_constant(
        spec.max_exponent()-spec.bias(), bv_utils.width(old_exponent));

    // the exponent is garbage if the fractional is zero

    literalt exponent_too_large=
      prop.land(
        prop.lnot(
          bv_utils.signed_less_than(old_exponent, max_exponent)),
        prop.lnot(bv_utils.is_zero(result.fraction)));

    result.infinity=prop.lor(result.infinity, exponent_too_large);
  }
}

/*******************************************************************\

Function: float_utilst::bias

  Inputs:

 Outputs:

 Purpose: takes an unbiased float, and applies the bias

\*******************************************************************/

float_utilst::biased_floatt float_utilst::bias(const unbiased_floatt &src)
{
  biased_floatt result;

  result.sign=src.sign;
  result.NaN=src.NaN;
  result.infinity=src.infinity;

  // we need to bias the new exponent
  result.exponent=add_bias(src.exponent);

  // strip off hidden bit
  assert(bv_utils.width(src.fraction)==spec.f+1);

  literalt hidden_bit=sign_bit(src.fraction);
  literalt denormal=prop.lnot(hidden_bit);

  result.fraction=bv_utils.extract(src.fraction, 0, spec.f-1);

  // make exponent zero if its denormal
  // (includes zero)
  result.exponent=prop.land(result.exponent,
      bv_utils.sign_extension(prop.lnot(denormal), bv_utils.width(result.exponent)));

  return result;
}

/*******************************************************************\

Function: float_utilst::add_bias

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::add_bias(const bvt &src)
{
  assert(bv_utils.width(src)==spec.e);

  return bv_utils.add(
    src,
    bv_utils.build_constant(spec.bias(), spec.e));
}

/*******************************************************************\

Function: float_utilst::sub_bias

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::sub_bias(const bvt &src)
{
  assert(bv_utils.width(src)==spec.e);

  return bv_utils.sub(
    src,
    bv_utils.build_constant(spec.bias(), spec.e));
}

/*******************************************************************\

Function: float_utilst::unpack

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

float_utilst::unbiased_floatt float_utilst::unpack(const bvt &src)
{
  assert(bv_utils.width(src)==spec.width());

  unbiased_floatt result;

  result.sign=sign_bit(src);

  result.fraction=get_fraction(src);
  // add hidden bit
  result.fraction=bv_utils.concatenate(result.fraction, is_normal(src));

  result.exponent=get_exponent(src);
  assert(bv_utils.width(result.exponent)==spec.e);

  // unbias the exponent
  literalt denormal=bv_utils.is_zero(result.exponent);

  result.exponent=
    bv_utils.select(denormal,
      bv_utils.build_constant(-spec.bias()+1, spec.e),
      sub_bias(result.exponent));

  result.infinity=is_infinity(src);
  result.zero=is_zero(src);
  result.NaN=is_NaN(src);

  return result;
}

/*******************************************************************\

Function: float_utilst::pack

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::pack(const biased_floatt &src)
{
  assert(bv_utils.width(src.fraction)==spec.f);
  assert(bv_utils.width(src.exponent)==spec.e);

  std::vector<literalt> result;
  result.resize(spec.width());

  // do sign
  // we make this 'false' for NaN
  result[result.size()-1]=
    prop.lselect(src.NaN, const_literal(false), src.sign);

  literalt infinity_or_NaN=
    prop.lor(src.NaN, src.infinity);

  // just copy fraction
  for(unsigned i=0; i<spec.f; i++)
    result[i]=prop.land(bv_utils.bit(src.fraction, i), prop.lnot(infinity_or_NaN));

  result[0]=prop.lor(result[0], src.NaN);

  // do exponent
  for(unsigned i=0; i<spec.e; i++)
    result[i+spec.f]=prop.lor(
      bv_utils.bit(src.exponent, i),
      infinity_or_NaN);

  return bv_utils.concatenate(result);
}

/*******************************************************************\

Function: float_utilst::sticky_right_shift

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::sticky_right_shift(
  const bvt &op,
  const bv_utilst::shiftt shift_type,
  const bvt &dist,
  literalt &sticky)
{
  // only right shifts
  assert(shift_type==bv_utilst::LRIGHT ||
         shift_type==bv_utilst::ARIGHT);

  unsigned d=1, width=bv_utils.width(op);
  std::vector<literalt> result;
  for (unsigned b=0; b!=bv_utils.width(op); ++b)
    result.push_back(bv_utils.bit(op, b));
  sticky=const_literal(false);

  for(unsigned stage=0; stage<bv_utils.width(dist); stage++)
  {
    literalt ds=bv_utils.bit(dist, stage);
    if(ds!=const_literal(false))
    {
      bvt rbv=bv_utils.concatenate(result);

      bvt tmp=bv_utils.shift(rbv, shift_type, d);

      bvt lost_bits=bv_utils.extract(rbv, 0, d-1);

      sticky=prop.lor(
          prop.land(ds,prop.lor(lost_bits)),
          sticky);

      for(unsigned i=0; i<width; i++)
        result[i]=prop.lselect(ds, bv_utils.bit(tmp, i), result[i]);
    }

    d=d<<1;
  }

  return bv_utils.concatenate(result);
}

/*******************************************************************\

Function: float_utilst::debug1

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::debug1(
  const bvt &src1,
  const bvt &src2)
{
  return src1;
}

/*******************************************************************\

Function: float_utilst::debug1

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

bvt float_utilst::debug2(
  const bvt &op0,
  const bvt &op1)
{
  return op0;
}

