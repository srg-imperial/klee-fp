//===-- simd-count.cpp - LLVM .bc SIMD instruction counter ----------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Internal/Module/SIMDRecognition.h"
#include "llvm/LLVMContext.h"
#include "llvm/Instructions.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/Intrinsics.h"
#include "llvm/Module.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/System/Signals.h"
#include <memory>
using namespace llvm;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Override output filename"),
               cl::value_desc("filename"));

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  
  LLVMContext &Context = getGlobalContext();
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  
  
  cl::ParseCommandLineOptions(argc, argv, "LLVM .bc SIMD instruction counter\n");

  std::string ErrorMessage;
  std::auto_ptr<Module> M;
 
  if (MemoryBuffer *Buffer
         = MemoryBuffer::getFileOrSTDIN(InputFilename, &ErrorMessage)) {
    M.reset(ParseBitcodeFile(Buffer, Context, &ErrorMessage));
    delete Buffer;
  }

  if (M.get() == 0) {
    errs() << argv[0] << ": ";
    if (ErrorMessage.size())
      errs() << ErrorMessage << "\n";
    else
      errs() << "bitcode didn't read correctly.\n";
    return 1;
  }
  
  if (OutputFilename.empty()) { // Unspecified output, infer it.
    if (InputFilename == "-") {
      OutputFilename = "-";
    } else {
      OutputFilename = InputFilename+".simd";
    }
  }

  // Make sure that the Out file gets unlinked from the disk if we get a
  // SIGINT.
  if (OutputFilename != "-")
    sys::RemoveFileOnSignal(sys::Path(OutputFilename));

  std::string ErrorInfo;
  std::auto_ptr<raw_fd_ostream> 
  Out(new raw_fd_ostream(OutputFilename.c_str(), ErrorInfo,
                         raw_fd_ostream::F_Binary));
  if (!ErrorInfo.empty()) {
    errs() << ErrorInfo << '\n';
    return 1;
  }

  for (Module::iterator f = M->begin(), fe = M->end(); f != fe; ++f)
    for (Function::iterator b = f->begin(), be = f->end(); b != be; ++b)
      for (BasicBlock::iterator i = b->begin(), ie = b->end(); i != ie; ++i)
        if (klee::isSIMDInstruction(i)) {
          *Out << i->getParent()->getParent()->getName() << "," << i->getParent()->getName() << ",";
          *Out << klee::getIntrinsicOrInstructionName(i);
          *Out << "\n";
        }

  return 0;
}

