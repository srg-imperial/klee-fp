//===-- SIMDRecognition.h -------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

namespace llvm {

class Instruction;
class StringRef;

}

namespace klee {

bool isSIMDInstruction(llvm::Instruction *i);
llvm::StringRef getIntrinsicOrInstructionName(llvm::Instruction *i);

}
