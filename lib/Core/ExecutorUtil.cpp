//===-- ExecutorUtil.cpp --------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Executor.h"

#include "Context.h"

#include "klee/Expr.h"
#include "klee/Interpreter.h"
#include "klee/Solver.h"

#include "klee/Internal/Module/KModule.h"

#include "llvm/Constants.h"
#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
#include "llvm/ModuleProvider.h"
#endif
#include "llvm/Support/CallSite.h"
#include "llvm/Support/GetElementPtrTypeIterator.h"
#include "llvm/Target/TargetData.h"
#include <iostream>
#include <cassert>

using namespace klee;
using namespace llvm;

namespace klee {

  ref<ConstantExpr> Executor::evalConstantExpr(llvm::ConstantExpr *ce) {
    const llvm::Type *type = ce->getType();

    ref<ConstantExpr> op1(0), op2(0), op3(0);
    IConstantExpr *iop1, *iop2, *iop3;
    int numOperands = ce->getNumOperands();

    if (numOperands > 0) op1 = evalConstant(ce->getOperand(0));
    if (numOperands > 1) op2 = evalConstant(ce->getOperand(1));
    if (numOperands > 2) op3 = evalConstant(ce->getOperand(2));

    iop1 = dyn_cast_or_null<IConstantExpr>(op1.get());
    iop2 = dyn_cast_or_null<IConstantExpr>(op2.get());
    iop3 = dyn_cast_or_null<IConstantExpr>(op3.get());

    switch (ce->getOpcode()) {
    default :
      ce->dump();
      std::cerr << "error: unknown ConstantExpr type\n"
                << "opcode: " << ce->getOpcode() << "\n";
      abort();

    case Instruction::Trunc: 
      return iop1->Extract(0, Expr::getWidthForLLVMType(type));
    case Instruction::ZExt:  return iop1->ZExt(Expr::getWidthForLLVMType(type));
    case Instruction::SExt:  return iop1->SExt(Expr::getWidthForLLVMType(type));
    case Instruction::Add:   return iop1->Add(iop2);
    case Instruction::Sub:   return iop1->Sub(iop2);
    case Instruction::Mul:   return iop1->Mul(iop2);
    case Instruction::SDiv:  return iop1->SDiv(iop2);
    case Instruction::UDiv:  return iop1->UDiv(iop2);
    case Instruction::SRem:  return iop1->SRem(iop2);
    case Instruction::URem:  return iop1->URem(iop2);
    case Instruction::And:   return iop1->And(iop2);
    case Instruction::Or:    return iop1->Or(iop2);
    case Instruction::Xor:   return iop1->Xor(iop2);
    case Instruction::Shl:   return iop1->Shl(iop2);
    case Instruction::LShr:  return iop1->LShr(iop2);
    case Instruction::AShr:  return iop1->AShr(iop2);
    case Instruction::BitCast:  return iop1;

    case Instruction::IntToPtr:
      return iop1->ZExt(Expr::getWidthForLLVMType(type));

    case Instruction::PtrToInt:
      return iop1->ZExt(Expr::getWidthForLLVMType(type));

    case Instruction::GetElementPtr: {
      ref<IConstantExpr> base = iop1->ZExt(Context::get().getPointerWidth());

      for (gep_type_iterator ii = gep_type_begin(ce), ie = gep_type_end(ce);
           ii != ie; ++ii) {
        ref<IConstantExpr> addend = 
          IConstantExpr::alloc(0, Context::get().getPointerWidth());

        if (const StructType *st = dyn_cast<StructType>(*ii)) {
          const StructLayout *sl = kmodule->targetData->getStructLayout(st);
          const ConstantInt *ci = cast<ConstantInt>(ii.getOperand());

          addend = IConstantExpr::alloc(sl->getElementOffset((unsigned)
                                                            ci->getZExtValue()),
                                       Context::get().getPointerWidth());
        } else {
          const SequentialType *set = cast<SequentialType>(*ii);
          ref<ConstantExpr> index = 
            evalConstant(cast<Constant>(ii.getOperand()));
          assert(isa<IConstantExpr>(index) && "array index not an integer?");
          ref<IConstantExpr> iIndex (cast<IConstantExpr>(index));

          unsigned elementSize = 
            kmodule->targetData->getTypeStoreSize(set->getElementType());

          iIndex = iIndex->ZExt(Context::get().getPointerWidth());
          addend = iIndex->Mul(IConstantExpr::alloc(elementSize, 
                                                  Context::get().getPointerWidth()));
        }

        base = base->Add(addend);
      }

      return base;
    }
      
    case Instruction::ICmp: {
      switch(ce->getPredicate()) {
      default: assert(0 && "unhandled ICmp predicate");
      case ICmpInst::ICMP_EQ:  return iop1->Eq(iop2);
      case ICmpInst::ICMP_NE:  return iop1->Ne(iop2);
      case ICmpInst::ICMP_UGT: return iop1->Ugt(iop2);
      case ICmpInst::ICMP_UGE: return iop1->Uge(iop2);
      case ICmpInst::ICMP_ULT: return iop1->Ult(iop2);
      case ICmpInst::ICMP_ULE: return iop1->Ule(iop2);
      case ICmpInst::ICMP_SGT: return iop1->Sgt(iop2);
      case ICmpInst::ICMP_SGE: return iop1->Sge(iop2);
      case ICmpInst::ICMP_SLT: return iop1->Slt(iop2);
      case ICmpInst::ICMP_SLE: return iop1->Sle(iop2);
      }
    }

    case Instruction::Select:
      return iop1->isTrue() ? iop2 : iop3;

    case Instruction::FAdd:
    case Instruction::FSub:
    case Instruction::FMul:
    case Instruction::FDiv:
    case Instruction::FRem:
    case Instruction::FPTrunc:
    case Instruction::FPExt:
    case Instruction::UIToFP:
    case Instruction::SIToFP:
    case Instruction::FPToUI:
    case Instruction::FPToSI:
    case Instruction::FCmp:
      assert(0 && "floating point ConstantExprs unsupported");
    }
  }

}
