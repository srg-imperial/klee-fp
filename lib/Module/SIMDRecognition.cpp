//===-- SIMDRecognition.cpp -----------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <klee/Internal/Module/SIMDRecognition.h>
#include <llvm/Instruction.h>
#include <llvm/Instructions.h>
#include <llvm/Intrinsics.h>
#include <llvm/IntrinsicInst.h>

using namespace llvm;

// Is this a SIMD instruction?
// The following operations are counted:
// 1) Any instruction of vector type
// 2) Any ExtractElement instruction
// 3) Stores of vector operand type
// 4) Casts from vector type
// 5) SSE intrinsics (name begins llvm.x86.mmx, llvm.x86.sse or llvm.x86.ssse)
bool klee::isSIMDInstruction(Instruction *i) {
  if (isa<VectorType>(i->getType()))
    return true;

  if (isa<ExtractElementInst>(i))
    return true;

  if (StoreInst *si = dyn_cast<StoreInst>(i)) {
    if (isa<VectorType>(si->getOperand(0)->getType()))
      return true;
  }

  if (CastInst *ci = dyn_cast<CastInst>(i)) {
    if (isa<VectorType>(ci->getSrcTy()))
      return true;
  }

  if (IntrinsicInst *ii = dyn_cast<IntrinsicInst>(i)) {
    std::string name = Intrinsic::getName(ii->getIntrinsicID());
    if (name.find("llvm.x86.mmx") == 0
     || name.find("llvm.x86.sse") == 0
     || name.find("llvm.x86.ssse") == 0) {
      return true;
    }
  }

  return false;
}

StringRef klee::getIntrinsicOrInstructionName(Instruction *i) {
  if (IntrinsicInst *ii = dyn_cast<IntrinsicInst>(i))
    return ii->getCalledFunction()->getName();
  else
    return i->getOpcodeName();
}
