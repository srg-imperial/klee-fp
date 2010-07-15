//===-- IntrinsicCleaner.cpp ----------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Passes.h"

#include "klee/Config/config.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/InstrTypes.h"
#include "llvm/Instruction.h"
#include "llvm/Instructions.h"
#include "llvm/IntrinsicInst.h"
#if !(LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
#include "llvm/LLVMContext.h"
#endif
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Type.h"
#include "llvm/Support/IRBuilder.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Target/TargetData.h"

using namespace llvm;

namespace klee {

char IntrinsicCleanerPass::ID;

bool IntrinsicCleanerPass::runOnModule(Module &M) {
  bool dirty = false;
  for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f)
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b)
      dirty |= runOnBasicBlock(*b);
  return dirty;
}

static Value *CreateSaturatedValue(IRBuilder<> &builder, bool isSigned, const IntegerType *tt, Value *val) {
  assert(isa<IntegerType>(val->getType()));
  const IntegerType *ft = cast<IntegerType>(val->getType());
  APInt maxVal = (isSigned ? APInt::getSignedMaxValue : APInt::getMaxValue)(tt->getBitWidth()).zext(ft->getBitWidth());
  Constant *maxValConst = ConstantInt::get(ft, maxVal);
  Value *cmp = isSigned ? builder.CreateICmpSGT(val, maxValConst) : builder.CreateICmpUGT(val, maxValConst);
  val = builder.CreateSelect(cmp, maxValConst, val);
  if (isSigned) {
    APInt minVal = APInt::getSignedMinValue(tt->getBitWidth()).sext(ft->getBitWidth());
    Constant *minValConst = ConstantInt::get(ft, minVal);
    Value *cmp = isSigned ? builder.CreateICmpSLT(val, minValConst) : builder.CreateICmpULT(val, minValConst);
    val = builder.CreateSelect(cmp, minValConst, val);
  }
  val = builder.CreateTrunc(val, tt);
  return val;
}

static Value *CreateSaturatedUnsignedSub(IRBuilder<> &builder, Value *l, Value *r) {
  assert(l->getType() == r->getType());
  Value *cmp = builder.CreateICmpUGT(l, r);
  Constant *zero = ConstantInt::get(l->getType(), 0);
  Value *sub = builder.CreateSub(l, r);
  Value *result = builder.CreateSelect(cmp, sub, zero);
  return result;
}

static Value *CreateSaturatedUnsignedAdd(IRBuilder<> &builder, Value *l, Value *r) {
  assert(l->getType() == r->getType());
  Value *notl = builder.CreateNot(l);
  Value *cmp = builder.CreateICmpUGT(notl, r);
  Constant *ones = ConstantInt::get(l->getType(), -1);
  Value *add = builder.CreateAdd(l, r);
  Value *result = builder.CreateSelect(cmp, add, ones);
  return result;
}

static Value *CreateIsNegative(IRBuilder<> &builder, Value *v) {
  const IntegerType *t = cast<IntegerType>(v->getType());
  return builder.CreateICmpNE(builder.CreateAShr(v, t->getBitWidth()-1), ConstantInt::get(t, 0));
}

static Value *CreateSaturatedSignedAdd(IRBuilder<> &builder, Value *l, Value *r) {
  const IntegerType *t = cast<IntegerType>(l->getType());
  assert(l->getType() == r->getType());
  Value *lNeg = CreateIsNegative(builder, l);
  Value *rNeg = CreateIsNegative(builder, r);
  Value *add = builder.CreateAdd(l, r);
  Value *addNeg = CreateIsNegative(builder, add);
  Value *over = builder.CreateAnd(builder.CreateNot(lNeg), builder.CreateAnd(builder.CreateNot(rNeg), addNeg));
  Value *under = builder.CreateAnd(lNeg, builder.CreateAnd(rNeg, builder.CreateNot(addNeg)));

  Constant *min = ConstantInt::get(t, APInt::getSignedMinValue(t->getBitWidth()));
  Constant *max = ConstantInt::get(t, APInt::getSignedMaxValue(t->getBitWidth()));
  Value *res = builder.CreateSelect(over, max,
               builder.CreateSelect(under, min, add));
  return res;
}

static Value *CreateSignExtendedICmp(IRBuilder<> &builder, CmpInst::Predicate p, Value *l, Value *r) {
  assert(l->getType() == r->getType());
  Value *cmp = builder.CreateICmp(p, l, r);
  Value *res = builder.CreateSExt(cmp, l->getType());
  return res;
}

static Value *CreateMinMax(IRBuilder<> &builder, bool isMax, bool isSigned, Value *l, Value *r) {
  Value *cmp = isSigned ? builder.CreateICmpSLT(l, r) : builder.CreateICmpULT(l, r);
  return builder.CreateSelect(cmp, isMax ? r : l, isMax ? l : r);
}

static Value *CreateAbsDiff(IRBuilder<> &builder, bool isSigned, const IntegerType *tt, Value *l, Value *r) {
  Value *lmr = builder.CreateSub(builder.CreateIntCast(l, tt, isSigned),
                                 builder.CreateIntCast(r, tt, isSigned));
  Value *lmrIsNeg = CreateIsNegative(builder, lmr);
  return builder.CreateSelect(lmrIsNeg, builder.CreateNeg(lmr), lmr);
}

static void CreateSSECallback(IRBuilder<> &builder, IntrinsicInst *ii) {
  Module *mod = ii->getParent()->getParent()->getParent();
  Constant *fc = mod->getOrInsertFunction("klee_sse", 
                                          builder.getVoidTy(), 
					  builder.getInt8PtrTy(),
                                          NULL);

  Constant *intrinNameStr = ConstantArray::get(mod->getContext(), 
					       ii->getCalledFunction()->getName());
  
  GlobalVariable *intrinName = new GlobalVariable(*mod, intrinNameStr->getType(), true,
						  GlobalValue::InternalLinkage,
						  intrinNameStr,
						  "__" + ii->getCalledFunction()->getName());

  Constant* indexes[] = { ConstantInt::getNullValue(builder.getInt32Ty()), 
			  ConstantInt::getNullValue(builder.getInt32Ty()) };
  Constant *intrinNamePtr =
    ConstantExpr::getGetElementPtr(intrinName, indexes,
				   sizeof(indexes)/sizeof(indexes[0]));
  
  builder.CreateCall(fc, intrinNamePtr);
}

bool IntrinsicCleanerPass::runOnBasicBlock(BasicBlock &b) { 
  bool dirty = false;
  
  unsigned WordSize = TargetData.getPointerSizeInBits() / 8;
  for (BasicBlock::iterator i = b.begin(), ie = b.end(); i != ie;) {     
    IntrinsicInst *ii = dyn_cast<IntrinsicInst>(&*i);
    // increment now since LowerIntrinsic deletion makes iterator invalid.
    ++i;  
    if(ii) {
      IRBuilder<> builder(ii->getParent(), ii);

      switch (ii->getIntrinsicID()) {
      case Intrinsic::vastart:
      case Intrinsic::vaend:
        break;
        
        // Lower vacopy so that object resolution etc is handled by
        // normal instructions.
        //
        // FIXME: This is much more target dependent than just the word size,
        // however this works for x86-32 and x86-64.
      case Intrinsic::vacopy: { // (dst, src) -> *((i8**) dst) = *((i8**) src)
        Value *dst = ii->getOperand(1);
        Value *src = ii->getOperand(2);

        if (WordSize == 4) {
          Type *i8pp = PointerType::getUnqual(PointerType::getUnqual(Type::getInt8Ty(getGlobalContext())));
          Value *castedDst = CastInst::CreatePointerCast(dst, i8pp, "vacopy.cast.dst", ii);
          Value *castedSrc = CastInst::CreatePointerCast(src, i8pp, "vacopy.cast.src", ii);
          Value *load = new LoadInst(castedSrc, "vacopy.read", ii);
          new StoreInst(load, castedDst, false, ii);
        } else {
          assert(WordSize == 8 && "Invalid word size!");
          Type *i64p = PointerType::getUnqual(Type::getInt64Ty(getGlobalContext()));
          Value *pDst = CastInst::CreatePointerCast(dst, i64p, "vacopy.cast.dst", ii);
          Value *pSrc = CastInst::CreatePointerCast(src, i64p, "vacopy.cast.src", ii);
          Value *val = new LoadInst(pSrc, std::string(), ii); new StoreInst(val, pDst, ii);
          Value *off = ConstantInt::get(Type::getInt64Ty(getGlobalContext()), 1);
          pDst = GetElementPtrInst::Create(pDst, off, std::string(), ii);
          pSrc = GetElementPtrInst::Create(pSrc, off, std::string(), ii);
          val = new LoadInst(pSrc, std::string(), ii); new StoreInst(val, pDst, ii);
          pDst = GetElementPtrInst::Create(pDst, off, std::string(), ii);
          pSrc = GetElementPtrInst::Create(pSrc, off, std::string(), ii);
          val = new LoadInst(pSrc, std::string(), ii); new StoreInst(val, pDst, ii);
        }
        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse_loadu_ps:
      case Intrinsic::x86_sse2_loadu_dq: {
        CreateSSECallback(builder, ii);

        Value *src = ii->getOperand(1);

        const VectorType* vecTy = cast<VectorType>(ii->getType());
        PointerType *vecPtrTy = PointerType::get(vecTy, 0);

        CastInst* pv = new BitCastInst(src, vecPtrTy, "", ii);
        LoadInst* v = new LoadInst(pv, "", false, ii);

        ii->replaceAllUsesWith(v);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse_storeu_ps:
      case Intrinsic::x86_sse2_storel_dq:
      case Intrinsic::x86_sse2_storeu_dq: {
        CreateSSECallback(builder, ii);

        Value *dst = ii->getOperand(1);
        Value *src = ii->getOperand(2);

        const VectorType* vecTy = cast<VectorType>(src->getType());
        PointerType *vecPtrTy = PointerType::get(vecTy, 0);

        CastInst* pv = new BitCastInst(dst, vecPtrTy, "", ii);
        new StoreInst(src, pv, false, ii);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_psll_dq_bs:
      case Intrinsic::x86_sse2_psrl_dq_bs: {
        CreateSSECallback(builder, ii);

        Value *src = ii->getOperand(1);
        Value *count = ii->getOperand(2);

        const Type *i128 = IntegerType::get(getGlobalContext(), 128);

        Value *srci = builder.CreateBitCast(src, i128);
        Value *count128 = builder.CreateZExt(count, i128);
        Value *countbits = builder.CreateShl(count128, 3);
        Value *resi = ii->getIntrinsicID() == Intrinsic::x86_sse2_psll_dq_bs ? builder.CreateShl(srci, countbits) : builder.CreateLShr(srci, countbits);
        Value *res = builder.CreateBitCast(resi, src->getType());

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_cvtdq2ps: {
        CreateSSECallback(builder, ii);

        Value *src = ii->getOperand(1);

        Value *res = builder.CreateSIToFP(src, ii->getType());

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_cvtps2dq: {
        CreateSSECallback(builder, ii);

        Value *src = ii->getOperand(1);

        Value *res = builder.CreateFPToSI(src, ii->getType());

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_cvtsd2si: {
        CreateSSECallback(builder, ii);

        const Type *i32 = Type::getInt32Ty(getGlobalContext());

        Value *zero32 = ConstantInt::get(i32, 0);

        Value *src = ii->getOperand(1);

        ExtractElementInst *lowElem = ExtractElementInst::Create(src, zero32, "", ii);
        FPToSIInst *conv = new FPToSIInst(lowElem, i32, "", ii);

        ii->replaceAllUsesWith(conv);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_mmx_packssdw:
      case Intrinsic::x86_sse2_packssdw_128:
      case Intrinsic::x86_mmx_packsswb:
      case Intrinsic::x86_sse2_packsswb_128:
      case Intrinsic::x86_mmx_packuswb:
      case Intrinsic::x86_sse2_packuswb_128: {
        CreateSSECallback(builder, ii);

        Value *src1 = ii->getOperand(1);
        Value *src2 = ii->getOperand(2);

        const VectorType *srcTy = cast<VectorType>(src1->getType());
        unsigned srcElCount = srcTy->getNumElements();

        assert(src2->getType() == srcTy);

        const VectorType *dstTy = cast<VectorType>(ii->getType());
        const IntegerType *dstElTy = cast<IntegerType>(dstTy->getElementType());
        unsigned dstElCount = dstTy->getNumElements();

        assert(srcElCount*2 == dstElCount);

        bool isSigned = (ii->getIntrinsicID() == Intrinsic::x86_mmx_packssdw
                      || ii->getIntrinsicID() == Intrinsic::x86_sse2_packssdw_128
                      || ii->getIntrinsicID() == Intrinsic::x86_mmx_packsswb
                      || ii->getIntrinsicID() == Intrinsic::x86_sse2_packsswb_128);

        const IntegerType *i32 = Type::getInt32Ty(getGlobalContext());

        Value *res = UndefValue::get(dstTy);

        for (unsigned i = 0; i < srcElCount; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          res = builder.CreateInsertElement(res,
                                            CreateSaturatedValue(builder, isSigned, dstElTy,
                                                                 builder.CreateExtractElement(src1, ic)),
                                            ic);
        }

        for (unsigned i = 0; i < srcElCount; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          Constant *icOfs = ConstantInt::get(i32, i+srcElCount);
          res = builder.CreateInsertElement(res,
                                            CreateSaturatedValue(builder, isSigned, dstElTy,
                                                                 builder.CreateExtractElement(src2, ic)),
                                            icOfs);
        }

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_pminu_b:
      case Intrinsic::x86_sse2_pmaxu_b:
      case Intrinsic::x86_sse2_pmins_w:
      case Intrinsic::x86_sse2_pmaxs_w: {
        CreateSSECallback(builder, ii);

        Value *src1 = ii->getOperand(1);
        Value *src2 = ii->getOperand(2);

        const VectorType *vt = cast<VectorType>(src1->getType());
        unsigned elCount = vt->getNumElements();

        assert(src2->getType() == vt);
        assert(ii->getType() == vt);

        bool isMax = (ii->getIntrinsicID() == Intrinsic::x86_sse2_pmaxu_b
                   || ii->getIntrinsicID() == Intrinsic::x86_sse2_pmaxs_w);
        bool isSigned = (ii->getIntrinsicID() == Intrinsic::x86_sse2_pmins_w
                      || ii->getIntrinsicID() == Intrinsic::x86_sse2_pmaxs_w);

        const IntegerType *i32 = Type::getInt32Ty(getGlobalContext());

        Value *res = UndefValue::get(vt);

        for (unsigned i = 0; i < elCount; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          res = builder.CreateInsertElement(res,
                                            CreateMinMax(builder, isMax, isSigned,
                                                         builder.CreateExtractElement(src1, ic),
                                                         builder.CreateExtractElement(src2, ic)),
                                            ic);
        }

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_psubus_b: {
        CreateSSECallback(builder, ii);

        Value *src1 = ii->getOperand(1);
        Value *src2 = ii->getOperand(2);

        const VectorType *vt = cast<VectorType>(src1->getType());
        unsigned elCount = vt->getNumElements();

        assert(src2->getType() == vt);
        assert(ii->getType() == vt);

        const IntegerType *i32 = Type::getInt32Ty(getGlobalContext());

        Value *res = UndefValue::get(vt);

        for (unsigned i = 0; i < elCount; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          res = builder.CreateInsertElement(res,
                                            CreateSaturatedUnsignedSub(builder,
                                                         builder.CreateExtractElement(src1, ic),
                                                         builder.CreateExtractElement(src2, ic)),
                                            ic);
        }

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_paddus_b: {
        CreateSSECallback(builder, ii);

        Value *src1 = ii->getOperand(1);
        Value *src2 = ii->getOperand(2);

        const VectorType *vt = cast<VectorType>(src1->getType());
        unsigned elCount = vt->getNumElements();

        assert(src2->getType() == vt);
        assert(ii->getType() == vt);

        const IntegerType *i32 = Type::getInt32Ty(getGlobalContext());

        Value *res = UndefValue::get(vt);

        for (unsigned i = 0; i < elCount; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          res = builder.CreateInsertElement(res,
                                            CreateSaturatedUnsignedAdd(builder,
                                                         builder.CreateExtractElement(src1, ic),
                                                         builder.CreateExtractElement(src2, ic)),
                                            ic);
        }

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_padds_w: {
        CreateSSECallback(builder, ii);

        Value *src1 = ii->getOperand(1);
        Value *src2 = ii->getOperand(2);

        const VectorType *vt = cast<VectorType>(src1->getType());
        unsigned elCount = vt->getNumElements();

        assert(src2->getType() == vt);
        assert(ii->getType() == vt);

        const IntegerType *i32 = Type::getInt32Ty(getGlobalContext());

        Value *res = UndefValue::get(vt);

        for (unsigned i = 0; i < elCount; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          res = builder.CreateInsertElement(res,
                                            CreateSaturatedSignedAdd(builder,
                                                         builder.CreateExtractElement(src1, ic),
                                                         builder.CreateExtractElement(src2, ic)),
                                            ic);
        }

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_pcmpgt_b:
      case Intrinsic::x86_sse2_pcmpgt_w: {
        CreateSSECallback(builder, ii);

        Value *src1 = ii->getOperand(1);
        Value *src2 = ii->getOperand(2);

        const VectorType *vt = cast<VectorType>(src1->getType());
        unsigned elCount = vt->getNumElements();

        assert(src2->getType() == vt);
        assert(ii->getType() == vt);

        const IntegerType *i32 = Type::getInt32Ty(getGlobalContext());

        Value *res = UndefValue::get(vt);

        for (unsigned i = 0; i < elCount; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          res = builder.CreateInsertElement(res,
                                            CreateSignExtendedICmp(builder, ICmpInst::ICMP_SGT,
                                                         builder.CreateExtractElement(src1, ic),
                                                         builder.CreateExtractElement(src2, ic)),
                                            ic);
        }

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_psrai_d:
      case Intrinsic::x86_sse2_psrai_w: {
        CreateSSECallback(builder, ii);

        Value *src = ii->getOperand(1);
        Value *count = ii->getOperand(2);

        const VectorType *vt = cast<VectorType>(src->getType());
        unsigned elCount = vt->getNumElements();

        assert(ii->getType() == vt);

        count = builder.CreateIntCast(count, vt->getElementType(), false);

        const IntegerType *i32 = Type::getInt32Ty(getGlobalContext());

        Value *res = UndefValue::get(vt);

        for (unsigned i = 0; i < elCount; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          res = builder.CreateInsertElement(res,
                                            builder.CreateAShr(builder.CreateExtractElement(src, ic), count),
                                            ic);
        }

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_pmulh_w: {
        CreateSSECallback(builder, ii);

        Value *src1 = ii->getOperand(1);
        Value *src2 = ii->getOperand(2);

        const VectorType *vt = cast<VectorType>(src1->getType());
        unsigned elCount = vt->getNumElements();

        const IntegerType *i16 = Type::getInt16Ty(getGlobalContext());
        const IntegerType *i32 = Type::getInt32Ty(getGlobalContext());

        assert(src2->getType() == vt);
        assert(ii->getType() == vt);
        assert(vt->getElementType() == i16);

        Value *res = UndefValue::get(vt);

        for (unsigned i = 0; i < elCount; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          Value *v1 = builder.CreateExtractElement(src1, ic);
          Value *v2 = builder.CreateExtractElement(src2, ic);
          Value *x1 = builder.CreateSExt(v1, i32);
          Value *x2 = builder.CreateSExt(v2, i32);
          Value *mul = builder.CreateMul(x1, x2);
          Value *mulHi = builder.CreateLShr(mul, 16);
          Value *mulHiTrunc = builder.CreateTrunc(mulHi, i16);
          res = builder.CreateInsertElement(res, mulHiTrunc, ic);
        }

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_psad_bw: {
        CreateSSECallback(builder, ii);

        Value *src1 = ii->getOperand(1);
        Value *src2 = ii->getOperand(2);

        const VectorType *vt = cast<VectorType>(src1->getType());
        const VectorType *rt = cast<VectorType>(ii->getType());

        const IntegerType *i8 = Type::getInt8Ty(getGlobalContext());
        const IntegerType *i16 = Type::getInt16Ty(getGlobalContext());
        const IntegerType *i32 = Type::getInt32Ty(getGlobalContext());
        const IntegerType *i64 = Type::getInt64Ty(getGlobalContext());

        assert(src2->getType() == vt);

        assert(vt->getElementType() == i8);
        assert(vt->getNumElements() == 16);

        assert(rt->getElementType() == i64);
        assert(rt->getNumElements() == 2);

        Value *res = UndefValue::get(rt);

        Value *lo = ConstantInt::get(i16, 0);
        for (unsigned i = 0; i < 8; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          Value *v1 = builder.CreateExtractElement(src1, ic);
          Value *v2 = builder.CreateExtractElement(src2, ic);
          lo = builder.CreateAdd(lo, CreateAbsDiff(builder, false, i16, v1, v2));
        }
        lo = builder.CreateZExt(lo, i64);
        res = builder.CreateInsertElement(res, lo, ConstantInt::get(i32, 0));

        Value *hi = ConstantInt::get(i16, 0);
        for (unsigned i = 8; i < 16; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          Value *v1 = builder.CreateExtractElement(src1, ic);
          Value *v2 = builder.CreateExtractElement(src2, ic);
          hi = builder.CreateAdd(hi, CreateAbsDiff(builder, false, i16, v1, v2));
        }
        hi = builder.CreateZExt(hi, i64);
        res = builder.CreateInsertElement(res, hi, ConstantInt::get(i32, 1));

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_pmadd_wd: {
        CreateSSECallback(builder, ii);

        Value *src1 = ii->getOperand(1);
        Value *src2 = ii->getOperand(2);

        const VectorType *vt = cast<VectorType>(src1->getType());
        const VectorType *rt = cast<VectorType>(ii->getType());

        const IntegerType *i32 = Type::getInt32Ty(getGlobalContext());

        assert(src2->getType() == vt);
        assert(vt->getNumElements() == rt->getNumElements()*2);

        Value *res = UndefValue::get(rt);

        for (unsigned i = 0; i < rt->getNumElements(); i++) {
          Constant *ic = ConstantInt::get(i32, i);
          Constant *i2p0c = ConstantInt::get(i32, i*2);
          Constant *i2p1c = ConstantInt::get(i32, i*2+1);
          Value *x0 = builder.CreateExtractElement(src1, i2p0c);
          Value *y0 = builder.CreateExtractElement(src2, i2p0c);
          Value *x1 = builder.CreateExtractElement(src1, i2p1c);
          Value *y1 = builder.CreateExtractElement(src2, i2p1c);
          x0 = builder.CreateIntCast(x0, rt->getElementType(), true);
          x1 = builder.CreateIntCast(x1, rt->getElementType(), true);
          y0 = builder.CreateIntCast(y0, rt->getElementType(), true);
          y1 = builder.CreateIntCast(y1, rt->getElementType(), true);
          Value *madd = builder.CreateAdd(builder.CreateMul(x1, y1),
                                          builder.CreateMul(x0, y0));
          res = builder.CreateInsertElement(res, madd, ic);
        }

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
      case Intrinsic::dbg_stoppoint: {
        // We can remove this stoppoint if the next instruction is
        // sure to be another stoppoint. This is nice for cleanliness
        // but also important for switch statements where it can allow
        // the targets to be joined.
        bool erase = false;
        if (isa<DbgStopPointInst>(i) ||
            isa<UnreachableInst>(i)) {
          erase = true;
        } else if (isa<BranchInst>(i) ||
                   isa<SwitchInst>(i)) {
          BasicBlock *bb = i->getParent();
          erase = true;
          for (succ_iterator it=succ_begin(bb), ie=succ_end(bb);
               it!=ie; ++it) {
            if (!isa<DbgStopPointInst>(it->getFirstNonPHI())) {
              erase = false;
              break;
            }
          }
        }

        if (erase) {
          ii->eraseFromParent();
          dirty = true;
        }
        break;
      }

      case Intrinsic::dbg_region_start:
      case Intrinsic::dbg_region_end:
      case Intrinsic::dbg_func_start:
#else
      case Intrinsic::dbg_value:
#endif
      case Intrinsic::dbg_declare:
        // Remove these regardless of lower intrinsics flag. This can
        // be removed once IntrinsicLowering is fixed to not have bad
        // caches.
        ii->eraseFromParent();
        dirty = true;
        break;
                    
      default:
        if (LowerIntrinsics)
          IL->LowerIntrinsicCall(ii);
        dirty = true;
        break;
      }
    }
  }

  return dirty;
}
}
