//===-- SIMDInstrumentation.cpp - instrument all SIMD instructions --------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Passes.h"

#include "klee/Config/config.h"
#include "klee/Internal/Module/InstructionInfoTable.h"
#include "klee/Internal/Module/SIMDRecognition.h"
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

#include <sstream>

using namespace llvm;

namespace klee {

char SIMDInstrumentationPass::ID;

bool SIMDInstrumentationPass::runOnModule(Module &M) {
  bool dirty = false;
  for (Module::iterator f = M.begin(), fe = M.end(); f != fe; ++f)
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b)
      dirty |= runOnBasicBlock(*b);
  return dirty;
}

static Constant *CreateStrConstPtr(Module *mod, StringRef str) {
  std::string strName = "__strconst_" + str.str();
  GlobalVariable *strVar = mod->getGlobalVariable(strName, true);

  if (!strVar) {
    Constant *strConst = ConstantArray::get(mod->getContext(), str);

    strVar = new GlobalVariable(*mod, strConst->getType(), true,
                                GlobalValue::InternalLinkage,
                                strConst, strName);
  }

  Constant *zero32 = ConstantInt::getNullValue(Type::getInt32Ty(mod->getContext()));
  Constant *indexes[] = { zero32, zero32 };

  Constant *strVarStartPtr =
    ConstantExpr::getGetElementPtr(strVar, indexes,
				   sizeof(indexes)/sizeof(indexes[0]));

  return strVarStartPtr;
}

/// Find instruction number by finding the instruction's place in
/// the basic block.
static unsigned InstructionNumber(Instruction *i) {
  BasicBlock *bb = i->getParent();
  unsigned instrNum = 0;
  for (BasicBlock::iterator ii = bb->begin(), ie = bb->end(); ii != ie; ++ii, ++instrNum) {
    if (&*ii == i)
      return instrNum;
  }
  assert(0 && "Could not find instruction in basic block!");
}

static void CreateSSECallback(IRBuilder<> &builder, Instruction *i,
                              const std::string &file, unsigned line) {
  Module *mod = i->getParent()->getParent()->getParent();
  Constant *fc = mod->getOrInsertFunction("klee_sse", 
                                          builder.getVoidTy(), 
                                          builder.getInt8PtrTy(),
                                          builder.getInt8PtrTy(),
                                          builder.getInt32Ty(),
                                          builder.getInt8PtrTy(),
                                          builder.getInt8PtrTy(),
                                          NULL);

  StringRef instrName = i->getName();
  std::ostringstream ss;
  ss << i->getParent()->getName().str() << " ";
  if (instrName.empty())
    ss << InstructionNumber(i);
  else
    ss << instrName.str();

  Value *args[] = {
    CreateStrConstPtr(mod, getIntrinsicOrInstructionName(i)),
    CreateStrConstPtr(mod, file),
    ConstantInt::get(builder.getInt32Ty(), line),
    CreateStrConstPtr(mod, i->getParent()->getParent()->getName()),
    CreateStrConstPtr(mod, ss.str())
  };

  builder.CreateCall(fc, args, args + sizeof(args)/sizeof(args[0]));
}

bool SIMDInstrumentationPass::runOnBasicBlock(BasicBlock &b) { 
  bool dirty = false;
  
  std::string file = "unknown";
  unsigned line = 0;
    
  for (BasicBlock::iterator i = b.begin(), ie = b.end(); i != ie; ++i) {     
    getInstructionDebugInfo(i, file, line);

    if(isSIMDInstruction(i)) {
      IRBuilder<> builder(i->getParent(), i);
      CreateSSECallback(builder, i, file, line);
      dirty = true;
    }
  }

  return dirty;
}
}
