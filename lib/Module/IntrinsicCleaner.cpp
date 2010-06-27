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
        Value *src = ii->getOperand(1);

        Value *res = builder.CreateSIToFP(src, ii->getType());

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_cvtps2dq: {
        Value *src = ii->getOperand(1);

        Value *res = builder.CreateFPToSI(src, ii->getType());

        ii->replaceAllUsesWith(res);

        ii->removeFromParent();
        delete ii;
        break;
      }

      case Intrinsic::x86_sse2_cvtsd2si: {
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

      case Intrinsic::x86_sse2_packssdw_128: {
        Value *src1 = ii->getOperand(1);
        Value *src2 = ii->getOperand(2);

        const IntegerType *i32 = Type::getInt32Ty(getGlobalContext());
        const IntegerType *i16 = Type::getInt16Ty(getGlobalContext());

        Value *res = UndefValue::get(ii->getType());

        for (unsigned i = 0; i < 3; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          res = builder.CreateInsertElement(res,
                                            CreateSaturatedValue(builder, true, i16,
                                                                 builder.CreateExtractElement(src1, ic)),
                                            ic);
        }

        for (unsigned i = 0; i < 3; i++) {
          Constant *ic = ConstantInt::get(i32, i);
          Constant *i4c = ConstantInt::get(i32, i+4);
          res = builder.CreateInsertElement(res,
                                            CreateSaturatedValue(builder, true, i16,
                                                                 builder.CreateExtractElement(src2, ic)),
                                            i4c);
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
