/*******************************************************************************
 * Copyright (C) 2010 Dependable Systems Laboratory, EPFL
 *
 * This file is part of the Cloud9-specific extensions to the KLEE symbolic
 * execution engine.
 *
 * Do NOT distribute this file to any third party; it is part of
 * unpublished work.
 *
 ******************************************************************************/

//===-- SpecialFunctionHandler.cpp ----------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Common.h"

#include "Memory.h"
#include "SpecialFunctionHandler.h"
#include "TimingSolver.h"

#include "klee/ExecutionState.h"

#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/KModule.h"
#include "klee/Internal/Support/ModuleUtil.h"

#include "klee/util/ExprCPrinter.h"

#include "Executor.h"
#include "MemoryManager.h"

#include "llvm/Type.h"
#include "llvm/InstrTypes.h"
#include "llvm/DerivedTypes.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/Twine.h"

#include "llvm/Support/MemoryBuffer.h"
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 9)
#include "llvm/System/Host.h"
#else
#include "llvm/Support/Host.h"
#endif
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 9)
#include "llvm/System/Path.h"
#else
#include "llvm/Support/Path.h"
#endif
#include "llvm/Support/raw_os_ostream.h"

#ifdef HAVE_OPENCL
#include "clang/Basic/Version.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"

#include "klee/Internal/CL/clintern.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <sys/syscall.h>

using namespace llvm;
using namespace klee;

llvm::cl::opt<bool>
DumpOpenCLSource("dump-opencl-source", llvm::cl::init(false));

llvm::cl::opt<bool>
DumpOpenCLModules("dump-opencl-modules", llvm::cl::init(false));

/// \todo Almost all of the demands in this file should be replaced
/// with terminateState calls.

///

struct HandlerInfo {
  const char *name;
  SpecialFunctionHandler::Handler handler;
  bool doesNotReturn; /// Intrinsic terminates the process
  bool hasReturnValue; /// Intrinsic has a return value
  bool doNotOverride; /// Intrinsic should not be used if already defined
};

// FIXME: We are more or less committed to requiring an intrinsic
// library these days. We can move some of this stuff there,
// especially things like realloc which have complicated semantics
// w.r.t. forking. Among other things this makes delayed query
// dispatch easier to implement.
HandlerInfo handlerInfo[] = {
#define add(name, handler, ret) { name, \
                                  &SpecialFunctionHandler::handler, \
                                  false, ret, false }
#define addDNR(name, handler) { name, \
                                &SpecialFunctionHandler::handler, \
                                true, false, false }
  addDNR("__assert_rtn", handleAssertFail),
  addDNR("__assert_fail", handleAssertFail),
  addDNR("_assert", handleAssert),
  addDNR("abort", handleAbort),
  addDNR("klee_abort", handleAbort),
  addDNR("klee_silent_exit", handleSilentExit),  
  addDNR("klee_report_error", handleReportError),
  addDNR("klee_thread_terminate", handleThreadTerminate),
  addDNR("klee_process_terminate", handleProcessTerminate),

  add("calloc", handleCalloc, true),
  add("free", handleFree, false),
  add("klee_assume", handleAssume, false),
  add("klee_check_memory_access", handleCheckMemoryAccess, false),
  add("klee_get_valuef", handleGetValue, true),
  add("klee_get_valued", handleGetValue, true),
  add("klee_get_valuel", handleGetValue, true),
  add("klee_get_valuell", handleGetValue, true),
  add("klee_get_value_i32", handleGetValue, true),
  add("klee_get_value_i64", handleGetValue, true),
  add("klee_define_fixed_object", handleDefineFixedObject, false),
  add("klee_get_obj_size", handleGetObjSize, true),
  add("klee_get_errno", handleGetErrno, true),
  add("klee_is_symbolic", handleIsSymbolic, true),
  add("klee_make_symbolic", handleMakeSymbolic, false),
  add("klee_mark_global", handleMarkGlobal, false),
  add("klee_merge", handleMerge, false),
  add("klee_prefer_cex", handlePreferCex, false),
  add("klee_print_expr", handlePrintExpr, false),
  add("klee_print_c_expr", handlePrintCExpr, false),
  add("klee_print_range", handlePrintRange, false),
  add("klee_set_forking", handleSetForking, false),
  add("klee_stack_trace", handleStackTrace, false),
  add("klee_dump_constraints", handleDumpConstraints, false),
  add("klee_watch", handleWatch, false),
  add("klee_warning", handleWarning, false),
  add("klee_warning_once", handleWarningOnce, false),
  add("klee_alias_function", handleAliasFunction, false),
  add("klee_ocl_compile", handleOclCompile, true),
  add("klee_ocl_get_arg_type", handleOclGetArgType, true),
  add("klee_ocl_get_arg_count", handleOclGetArgCount, true),
  add("klee_lookup_module_global", handleLookupModuleGlobal, true),
  add("klee_icall_create_arg_list", handleICallCreateArgList, true),
  add("klee_icall_add_arg", handleICallAddArg, false),
  add("klee_icall", handleICall, false),
  add("klee_icall_destroy_arg_list", handleICallDestroyArgList, false),
  add("klee_create_work_group", handleCreateWorkGroup, true),
  add("klee_set_work_group_id", handleSetWorkGroupId, false),

  add("klee_make_shared", handleMakeShared, false),
  add("klee_get_context", handleGetContext, false),
  add("klee_get_wlist", handleGetWList, true),
  add("klee_thread_preempt", handleThreadPreempt, false),
  add("klee_thread_sleep", handleThreadSleep, false),
  add("klee_thread_notify", handleThreadNotify, false),
  add("klee_thread_barrier", handleThreadBarrier, false),
  add("klee_warning", handleWarning, false),
  add("klee_warning_once", handleWarningOnce, false),
  add("klee_alias_function", handleAliasFunction, false),

  add("klee_thread_create", handleThreadCreate, false),
  add("klee_process_fork", handleProcessFork, true),

  add("klee_branch", handleBranch, true),
  add("klee_fork", handleFork, true),

  add("klee_debug", handleDebug, false),

  add("klee_get_time", handleGetTime, true),
  add("klee_set_time", handleSetTime, false),

  add("klee_sqrt", handleSqrt, true),
  add("klee_sqrtf", handleSqrt, true),

  add("klee_cos", handleCos, true),
  add("klee_cosf", handleCos, true),

  add("klee_sin", handleSin, true),
  add("klee_sinf", handleSin, true),

  add("malloc", handleMalloc, true),
  add("klee_malloc", handleMalloc, true),
  add("realloc", handleRealloc, true),
  add("valloc", handleValloc, true),

  // operator delete[](void*)
  add("_ZdaPv", handleDeleteArray, false),
  // operator delete(void*)
  add("_ZdlPv", handleDelete, false),

  // operator new[](unsigned int)
  add("_Znaj", handleNewArray, true),
  // operator new(unsigned int)
  add("_Znwj", handleNew, true),
  add("_ZnamRKSt9nothrow_t", handleNew, true),

  // FIXME-64: This is wrong for 64-bit long...

  // operator new[](unsigned long)
  add("_Znam", handleNewArray, true),
  // operator new(unsigned long)
  add("_Znwm", handleNew, true),
  add("_ZnwmRKSt9nothrow_t", handleNew, true),

  add("syscall", handleSyscall, true)

#undef addDNR
#undef add  
};

SpecialFunctionHandler::SpecialFunctionHandler(Executor &_executor) 
  : executor(_executor) {}


void SpecialFunctionHandler::prepare(KModule *kmodule) {
  unsigned N = sizeof(handlerInfo)/sizeof(handlerInfo[0]);

  for (unsigned i=0; i<N; ++i) {
    HandlerInfo &hi = handlerInfo[i];
    Function *f = kmodule->module->getFunction(hi.name);
    
    // No need to create if the function doesn't exist, since it cannot
    // be called in that case.
  
    if (f && (!hi.doNotOverride || f->isDeclaration())) {
      // Make sure NoReturn attribute is set, for optimization and
      // coverage counting.
      if (hi.doesNotReturn)
        f->addFnAttr(Attribute::NoReturn);

      // Change to a declaration since we handle internally (simplifies
      // module and allows deleting dead code).
      if (!f->isDeclaration())
        f->deleteBody();
    }
  }
}

void SpecialFunctionHandler::bind(KModule *kmodule) {
  unsigned N = sizeof(handlerInfo)/sizeof(handlerInfo[0]);

  for (unsigned i=0; i<N; ++i) {
    HandlerInfo &hi = handlerInfo[i];
    Function *f = kmodule->module->getFunction(hi.name);
    
    if (f && (!hi.doNotOverride || f->isDeclaration()))
      handlers[f] = std::make_pair(hi.handler, hi.hasReturnValue);
  }
}


bool SpecialFunctionHandler::handle(ExecutionState &state, 
                                    Function *f,
                                    KInstruction *target,
                                    std::vector< ref<Expr> > &arguments) {
  handlers_ty::iterator it = handlers.find(f);
  if (it != handlers.end()) {    
    Handler h = it->second.first;
    bool hasReturnValue = it->second.second;
     // FIXME: Check this... add test?
    if (!hasReturnValue && !target->inst->use_empty()) {
      executor.terminateStateOnExecError(state, 
                                         "expected return value from void special function");
    } else {
      (this->*h)(state, target, arguments);
    }
    return true;
  } else {
    return false;
  }
}

void SpecialFunctionHandler::processMemoryLocation(ExecutionState &state,
    ref<Expr> address, ref<Expr> size,
    const std::string &name, resolutions_ty &resList) {
  Executor::ExactResolutionList rl;
  executor.resolveExact(state, address, rl, name);

  for (Executor::ExactResolutionList::iterator it = rl.begin(),
         ie = rl.end(); it != ie; ++it) {
    const MemoryObject *mo = it->first.first;
    const ObjectState *os = it->first.second;
    ExecutionState *s = it->second;

    // FIXME: Type coercion should be done consistently somewhere.
    bool res;
    bool success =
      executor.solver->mustBeTrue(*s,
                                  EqExpr::create(ZExtExpr::create(size,
                                                                  Context::get().getPointerWidth()),
                                                 mo->getSizeExpr()),
                                  res);
    assert(success && "FIXME: Unhandled solver failure");

    if (res) {
      resList.push_back(std::make_pair(std::make_pair(mo, os), s));
    } else {
      executor.terminateStateOnError(*s,
                                     "wrong size given to memory operation",
                                     "user.err");
    }
  }
}

bool SpecialFunctionHandler::writeConcreteValue(ExecutionState &state,
        ref<Expr> address, uint64_t value, Expr::Width width) {
  ObjectPair op;

  if (!state.addressSpace().resolveOne(cast<ConstantExpr>(address), op)) {
    executor.terminateStateOnError(state, "invalid pointer for writing concrete value into", "user.err");
    return false;
  }

  ObjectState *os = state.addressSpace().getWriteable(op.first, op.second);

  os->write(op.first->getOffsetExpr(address), ConstantExpr::create(value, width), state.crtThread().getTid());

  return true;
}

/****/

// reads a concrete string from memory
std::string 
SpecialFunctionHandler::readStringAtAddress(ExecutionState &state, 
                                            ref<Expr> addressExpr) {
  ObjectPair op;
  addressExpr = executor.toUnique(state, addressExpr);
  ref<ConstantExpr> address = cast<ConstantExpr>(addressExpr);
  if (!state.addressSpace().resolveOne(address, op))
    assert(0 && "XXX out of bounds / multiple resolution unhandled");

  const MemoryObject *mo = op.first;
  const ObjectState *os = op.second;

  char *buf = new char[mo->size];
  unsigned ioffset = 0;

  ref<Expr> offset_expr = SubExpr::create(address, op.first->getBaseExpr());
  if (isa<ConstantExpr>(offset_expr)) {
	  ref<ConstantExpr> value = cast<ConstantExpr>(offset_expr.get());
  	  ioffset = value.get()->getZExtValue();
  } else
	  assert(0 && "Error: invalid interior pointer");

  assert(ioffset < mo->size);

  unsigned i;
  for (i = 0; i < mo->size - ioffset - 1; i++) {
    ref<Expr> cur = os->read8(i + ioffset, state.crtThread().getTid());
    cur = executor.toUnique(state, cur);
    if (!isa<ConstantExpr>(cur)) //XXX: Should actually concretize the value...
           return std::string("hit symbolic char while reading concrete string");
    buf[i] = cast<ConstantExpr>(cur)->getZExtValue(8);
  }
  buf[i] = 0;
  
  std::string result(buf);
  delete[] buf;
  return result;
}

// reads a memory block from memory
void
SpecialFunctionHandler::readMemoryAtAddress(ExecutionState &state, 
                                            ref<Expr> addressExpr,
                                            void *data,
                                            size_t size) {
  ObjectPair op;
  addressExpr = executor.toUnique(state, addressExpr);
  ref<ConstantExpr> address = cast<ConstantExpr>(addressExpr);
  if (!state.addressSpace().resolveOne(address, op))
    assert(0 && "XXX out of bounds / multiple resolution unhandled");
  bool res;
  assert(executor.solver->mustBeTrue(state, 
                                     EqExpr::create(address, 
                                                    op.first->getBaseExpr()),
                                     res) &&
         res &&
         "XXX interior pointer unhandled");
  const MemoryObject *mo = op.first;
  const ObjectState *os = op.second;

  assert(size < mo->size && "Memory object too small for data");
  char *buf = (char *) data;

  unsigned i;
  for (i = 0; i < mo->size; i++) {
    ref<Expr> cur = os->read8(i, state.crtThread().getTid());
    cur = executor.toUnique(state, cur);
    assert(isa<ConstantExpr>(cur) && 
           "hit symbolic char while reading concrete memory");
    buf[i] = cast<ConstantExpr>(cur)->getZExtValue(8);
  }
}

// writes a memory block to memory
void
SpecialFunctionHandler::writeMemoryAtAddress(ExecutionState &state, 
                                             ref<Expr> addressExpr,
                                             const void *data,
                                             size_t size) {
  ObjectPair op;
  addressExpr = executor.toUnique(state, addressExpr);
  ref<ConstantExpr> address = cast<ConstantExpr>(addressExpr);
  if (!state.addressSpace().resolveOne(address, op))
    assert(0 && "XXX out of bounds / multiple resolution unhandled");
  bool res;
  assert(executor.solver->mustBeTrue(state, 
                                     EqExpr::create(address, 
                                                    op.first->getBaseExpr()),
                                     res) &&
         res &&
         "XXX interior pointer unhandled");
  const MemoryObject *mo = op.first;
  ObjectState *os = const_cast<ObjectState *>(op.second);

  assert(size < mo->size && "Memory object too small for data");
  const char *buf = (const char *) data;

  unsigned i;
  for (i = 0; i < mo->size; i++) {
    os->write8(i, buf[i], state.crtThread().getTid());
  }
}

/****/

void SpecialFunctionHandler::handleAbort(ExecutionState &state,
                           KInstruction *target,
                           std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==0 && "invalid number of arguments to abort");


  executor.terminateStateOnError(state, "abort failure", "abort.err");
}

void SpecialFunctionHandler::handleSilentExit(ExecutionState &state,
                                              KInstruction *target,
                                              std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 && "invalid number of arguments to exit");

  executor.terminateState(state);
}

void SpecialFunctionHandler::handleAliasFunction(ExecutionState &state,
						 KInstruction *target,
						 std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==2 && 
         "invalid number of arguments to klee_alias_function");
  std::string old_fn = readStringAtAddress(state, arguments[0]);
  std::string new_fn = readStringAtAddress(state, arguments[1]);
  //std::cerr << "Replacing " << old_fn << "() with " << new_fn << "()\n";
  if (old_fn == new_fn)
    state.removeFnAlias(old_fn);
  else state.addFnAlias(old_fn, new_fn);
}

void SpecialFunctionHandler::handleAssert(ExecutionState &state,
                                          KInstruction *target,
                                          std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==3 && "invalid number of arguments to _assert");  
  

  executor.terminateStateOnError(state,
                                 "ASSERTION FAIL: " + readStringAtAddress(state, arguments[0]),
                                 "assert.err");
}

void SpecialFunctionHandler::handleAssertFail(ExecutionState &state,
                                              KInstruction *target,
                                              std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==4 && "invalid number of arguments to __assert_fail");
  

  executor.terminateStateOnError(state,
                                 "ASSERTION FAIL: " + readStringAtAddress(state, arguments[0]),
                                 "assert.err");
}

void SpecialFunctionHandler::handleReportError(ExecutionState &state,
                                               KInstruction *target,
                                               std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==4 && "invalid number of arguments to klee_report_error");
  
  // arguments[0], arguments[1] are file, line
  
  executor.terminateStateOnError(state,
                                 readStringAtAddress(state, arguments[2]),
                                 readStringAtAddress(state, arguments[3]).c_str());
}

void SpecialFunctionHandler::handleMerge(ExecutionState &state,
                           KInstruction *target,
                           std::vector<ref<Expr> > &arguments) {
  // nop
}

void SpecialFunctionHandler::handleNew(ExecutionState &state,
                         KInstruction *target,
                         std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()>=1 && "invalid number of arguments to new");

  executor.executeAlloc(state, arguments[0], false, target);
}

void SpecialFunctionHandler::handleDelete(ExecutionState &state,
                            KInstruction *target,
                            std::vector<ref<Expr> > &arguments) {
  // FIXME: Should check proper pairing with allocation type (malloc/free,
  // new/delete, new[]/delete[]).

  // XXX should type check args
  assert(arguments.size()==1 && "invalid number of arguments to delete");
  executor.executeFree(state, arguments[0]);
}

void SpecialFunctionHandler::handleNewArray(ExecutionState &state,
                              KInstruction *target,
                              std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==1 && "invalid number of arguments to new[]");
  executor.executeAlloc(state, arguments[0], false, target);
}

void SpecialFunctionHandler::handleDeleteArray(ExecutionState &state,
                                 KInstruction *target,
                                 std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==1 && "invalid number of arguments to delete[]");
  executor.executeFree(state, arguments[0]);
}

void SpecialFunctionHandler::handleMalloc(ExecutionState &state,
                                  KInstruction *target,
                                  std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert((arguments.size()==1 || arguments.size()==2) &&
         "invalid number of arguments to malloc");
  unsigned addrspace = 0;
  if (arguments.size() == 2) {
    ConstantExpr *addrspaceCE = dyn_cast<ConstantExpr>(arguments[1]);
    if (!addrspaceCE) {
      executor.terminateStateOnError(state, "symbolic address space",
                                     "user.err");
      return;
    }
    addrspace = addrspaceCE->getZExtValue();
  }
  executor.executeAlloc(state, arguments[0], false, target, addrspace);
}


void SpecialFunctionHandler::handleValloc(ExecutionState &state, 
					  KInstruction *target, 
					  std::vector<ref<Expr> > &arguments) {
  
  // XXX ignoring for now the "multiple of page size " requirement 
  //- executing the regular alloc
  // XXX should type check args
  assert(arguments.size() == 1 && "invalid number of arguments to valloc");
  executor.executeAlloc(state, arguments[0], false, target);
}

void SpecialFunctionHandler::handleAssume(ExecutionState &state,
                            KInstruction *target,
                            std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 && "invalid number of arguments to klee_assume");
  
  ref<Expr> e = arguments[0];
  
  if (e->getWidth() != Expr::Bool)
    e = NeExpr::create(e, ConstantExpr::create(0, e->getWidth()));
  
  bool res;
  bool success = executor.solver->mustBeFalse(state, e, res);
  assert(success && "FIXME: Unhandled solver failure");
  if (res) {
    executor.terminateStateOnError(state, 
                                   "invalid klee_assume call (provably false)",
                                   "user.err");
  } else {
    executor.addConstraint(state, e);
  }
}

void SpecialFunctionHandler::handleIsSymbolic(ExecutionState &state,
                                KInstruction *target,
                                std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 && "invalid number of arguments to klee_is_symbolic");

  //arguments[0]->print(std::cerr);

  executor.bindLocal(target, state, 
                     ConstantExpr::create(!isa<ConstantExpr>(arguments[0]),
                                          Expr::Int32));
}

void SpecialFunctionHandler::handlePreferCex(ExecutionState &state,
                                             KInstruction *target,
                                             std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==2 &&
         "invalid number of arguments to klee_prefex_cex");

  ref<Expr> cond = arguments[1];
  if (cond->getWidth() != Expr::Bool)
    cond = NeExpr::create(cond, ConstantExpr::alloc(0, cond->getWidth()));

  Executor::ExactResolutionList rl;
  executor.resolveExact(state, arguments[0], rl, "prefex_cex");
  
  assert(rl.size() == 1 &&
         "prefer_cex target must resolve to precisely one object");

  rl[0].first.first->cexPreferences.push_back(cond);
}

void SpecialFunctionHandler::handlePrintExpr(ExecutionState &state,
                                  KInstruction *target,
                                  std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==2 &&
         "invalid number of arguments to klee_print_expr");

  std::string msg_str = readStringAtAddress(state, arguments[0]);
  std::cerr << msg_str << ":" << arguments[1] << std::endl;

  for (ConstraintManager::constraint_iterator it = state.constraints().begin();
      it != state.constraints().end(); it++) {
    std::cerr << *it << std::endl;
  }
}

void SpecialFunctionHandler::handlePrintCExpr(ExecutionState &state,
                                           KInstruction *target,
                                           std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==3 &&
         "invalid number of arguments to klee_print_c_expr");

  ExprCPrinter::printExprEvaluator(std::cerr, arguments[2]);
}

void SpecialFunctionHandler::handleSetForking(ExecutionState &state,
                                              KInstruction *target,
                                              std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 &&
         "invalid number of arguments to klee_set_forking");
  ref<Expr> value = executor.toUnique(state, arguments[0]);
  
  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(value)) {
    state.forkDisabled = CE->isZero();
  } else {
    executor.terminateStateOnError(state, 
                                   "klee_set_forking requires a constant arg",
                                   "user.err");
  }
}

void SpecialFunctionHandler::handleStackTrace(ExecutionState &state,
                                              KInstruction *target,
                                              std::vector<ref<Expr> > &arguments) {
  state.crtThread().getStackTrace().dump(std::cout);
}

void SpecialFunctionHandler::handleDumpConstraints(ExecutionState &state,
                                                   KInstruction *target,
                                                   std::vector<ref<Expr> > &arguments) {
  state.constraints().dump(std::cout);
}

void SpecialFunctionHandler::handleWatch(ExecutionState &state,
                                         KInstruction *target,
                                         std::vector<ref<Expr> > &arguments) {
  state.watchpoint = arguments[0];
  assert(isa<ConstantExpr>(arguments[1]) && "Size param must be constant");
  state.watchpointSize = cast<ConstantExpr>(arguments[1])->getZExtValue();
}

void SpecialFunctionHandler::handleWarning(ExecutionState &state,
                                           KInstruction *target,
                                           std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 && "invalid number of arguments to klee_warning");

  std::string msg_str = readStringAtAddress(state, arguments[0]);
  klee_warning("%s: %s", state.stack().back().kf->function->getName().data(),
               msg_str.c_str());
}

void SpecialFunctionHandler::handleDebug(ExecutionState &state,
                                           KInstruction *target,
                                           std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() >= 1 && "invalid number of arguments to klee_debug");

  std::string formatStr = readStringAtAddress(state, arguments[0]);

  // XXX Ugly hack, need to use libffi here... Ashamed of myself

  if (arguments.size() == 2 && arguments[1]->getWidth() == sizeof(long)*8) {
    // Special case for displaying strings
    if (!isa<ConstantExpr>(arguments[1])) {
      executor.terminateStateOnError(state, "klee_debug needs a constant list of arguments", "user.err");
      return;
    }

    std::string paramStr = readStringAtAddress(state, arguments[1]);

    fprintf(stderr, formatStr.c_str(), paramStr.c_str());
    return;
  }

  std::vector<int> args;

  for (unsigned int i = 1; i < arguments.size(); i++) {
    if (!isa<ConstantExpr>(arguments[i])) {
      executor.terminateStateOnError(state, "klee_debug needs a constant list of arguments", "user.err");
      return;
    }

    ref<ConstantExpr> arg = cast<ConstantExpr>(arguments[i]);

    if (arg->getWidth() != sizeof(int)*8) {
      executor.terminateStateOnError(state, "klee_debug works only with 32-bit arguments", "user.err");
      return;
    }

    args.push_back((int)arg->getZExtValue());
  }

  switch (args.size()) {
  case 0:
    fprintf(stderr, "%s", formatStr.c_str());
    break;
  case 1:
    fprintf(stderr, formatStr.c_str(), args[0]);
    break;
  case 2:
    fprintf(stderr, formatStr.c_str(), args[0], args[1]);
    break;
  case 3:
    fprintf(stderr, formatStr.c_str(), args[0], args[1], args[2]);
    break;
  default:
    executor.terminateStateOnError(state, "klee_debug allows up to 3 arguments", "user.err");
    return;
  }

  //vprintf(formatStr.c_str(), *((va_list*)cast<ConstantExpr>(arguments[1])->getZExtValue()));
}

void SpecialFunctionHandler::handleWarningOnce(ExecutionState &state,
                                               KInstruction *target,
                                               std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 &&
         "invalid number of arguments to klee_warning_once");

  std::string msg_str = readStringAtAddress(state, arguments[0]);
  klee_warning_once(0, "%s: %s", state.stack().back().kf->function->getName().data(),
                    msg_str.c_str());
}

void SpecialFunctionHandler::handlePrintRange(ExecutionState &state,
                                  KInstruction *target,
                                  std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==2 &&
         "invalid number of arguments to klee_print_range");

  std::string msg_str = readStringAtAddress(state, arguments[0]);
  std::cerr << msg_str << ":" << arguments[1];
  if (!isa<ConstantExpr>(arguments[1])) {
    // FIXME: Pull into a unique value method?
    ref<ConstantExpr> value;
    bool success = executor.solver->getValue(state, arguments[1], value);
    assert(success && "FIXME: Unhandled solver failure");
    bool res;
    success = executor.solver->mustBeTrue(state, 
                                          EqExpr::create(arguments[1], value), 
                                          res);
    assert(success && "FIXME: Unhandled solver failure");
    if (res) {
      std::cerr << " == " << value;
    } else { 
      std::cerr << " ~= " << value;
      std::pair< ref<Expr>, ref<Expr> > res =
        executor.solver->getRange(state, arguments[1]);
      std::cerr << " (in [" << res.first << ", " << res.second <<"])";
    }
  }
  std::cerr << "\n";
}

void SpecialFunctionHandler::handleGetObjSize(ExecutionState &state,
                                  KInstruction *target,
                                  std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==1 &&
         "invalid number of arguments to klee_get_obj_size");
  Executor::ExactResolutionList rl;
  executor.resolveExact(state, arguments[0], rl, "klee_get_obj_size");
  for (Executor::ExactResolutionList::iterator it = rl.begin(), 
         ie = rl.end(); it != ie; ++it) {
    executor.bindLocal(target, *it->second, 
                       ConstantExpr::create(it->first.first->size, Expr::Int32));
  }
}

void SpecialFunctionHandler::handleGetErrno(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==0 &&
         "invalid number of arguments to klee_get_errno");
  executor.bindLocal(target, state,
                     ConstantExpr::create(errno, Expr::Int32));
}

void SpecialFunctionHandler::handleCalloc(ExecutionState &state,
                            KInstruction *target,
                            std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==2 &&
         "invalid number of arguments to calloc");

  ref<Expr> size = MulExpr::create(arguments[0],
                                   arguments[1]);
  executor.executeAlloc(state, size, false, target, true);
}

void SpecialFunctionHandler::handleRealloc(ExecutionState &state,
                            KInstruction *target,
                            std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==2 &&
         "invalid number of arguments to realloc");
  ref<Expr> address = arguments[0];
  ref<Expr> size = arguments[1];

  Executor::StatePair zeroSize = executor.fork(state, 
                                               Expr::createIsZero(size), 
                                               true, KLEE_FORK_INTERNAL);
  
  if (zeroSize.first) { // size == 0
    executor.executeFree(*zeroSize.first, address, target);   
  }
  if (zeroSize.second) { // size != 0
    Executor::StatePair zeroPointer = executor.fork(*zeroSize.second, 
                                                    Expr::createIsZero(address), 
                                                    true, KLEE_FORK_INTERNAL);
    
    if (zeroPointer.first) { // address == 0
      executor.executeAlloc(*zeroPointer.first, size, false, target);
    } 
    if (zeroPointer.second) { // address != 0
      Executor::ExactResolutionList rl;
      executor.resolveExact(*zeroPointer.second, address, rl, "realloc");
      
      for (Executor::ExactResolutionList::iterator it = rl.begin(), 
             ie = rl.end(); it != ie; ++it) {
        executor.executeAlloc(*it->second, size, false, target, 0, false, 
                              it->first.second);
      }
    }
  }
}

void SpecialFunctionHandler::handleFree(ExecutionState &state,
                          KInstruction *target,
                          std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size()==1 &&
         "invalid number of arguments to free");
  executor.executeFree(state, arguments[0]);
}

void SpecialFunctionHandler::handleMakeShared(ExecutionState &state,
                          KInstruction *target,
                          std::vector<ref<Expr> > &arguments) {

  assert(arguments.size() == 2 &&
        "invalid number of arguments to klee_make_shared");

  resolutions_ty resList;

  processMemoryLocation(state, arguments[0], arguments[1], "make_shared", resList);

  for (resolutions_ty::iterator it = resList.begin(); it != resList.end();
      it++) {
    const MemoryObject *mo = it->first.first;
    const ObjectState *os = it->first.second;
    ExecutionState *s = it->second;

    if (mo->isLocal) {
      executor.terminateStateOnError(*s,
                                     "cannot share local object",
                                     "user.err");
      continue;
    }

    unsigned int bindCount = 0;
    for (ExecutionState::processes_ty::iterator pit = s->processes.begin();
        pit != s->processes.end(); pit++) {
      if (pit->second.addressSpace.findObject(mo) != NULL)
        bindCount++;
    }

    if (bindCount != 1) {
      executor.terminateStateOnError(*s, "cannot shared already forked object",
           "user.err");
      continue;
    }

    ObjectState *newOS = state.addressSpace().getWriteable(mo, os);
    newOS->isShared = true;

    // Now bind this object in the other address spaces
    for (ExecutionState::processes_ty::iterator pit = s->processes.begin();
        pit != s->processes.end(); pit++) {
      if (pit == s->crtProcessIt)
        continue; // Skip the current process

      pit->second.addressSpace.bindSharedObject(mo, newOS);
    }
  }
}

void SpecialFunctionHandler::handleGetContext(ExecutionState &state,
                          KInstruction *target,
                          std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 &&
      "invalid number of arguments to klee_get_context");

  ref<Expr> tidAddr = executor.toUnique(state, arguments[0]);
  ref<Expr> pidAddr = executor.toUnique(state, arguments[1]);

  if (!isa<ConstantExpr>(tidAddr) || !isa<ConstantExpr>(pidAddr)) {
    executor.terminateStateOnError(state,
                                   "klee_get_context requires constant args",
                                   "user.err");
    return;
  }

  if (!tidAddr->isZero()) {
    if (!writeConcreteValue(state, tidAddr, state.crtThread().getTid(),
        executor.getWidthForLLVMType(executor.kmodule(state),
                                     Type::getInt64Ty(getGlobalContext()))))
      return;
  }

  if (!pidAddr->isZero()) {
    if (!writeConcreteValue(state, pidAddr, state.crtProcess().pid,
        executor.getWidthForLLVMType(executor.kmodule(state),
                                     Type::getInt32Ty(getGlobalContext()))))
      return;
  }
}

void SpecialFunctionHandler::handleGetTime(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.empty() && "invalid number of arguments to klee_get_time");

  executor.bindLocal(target, state, ConstantExpr::create(state.stateTime,
      executor.getWidthForLLVMType(executor.kmodule(state),
                                   target->inst->getType())));
}

void SpecialFunctionHandler::handleSetTime(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 && "invalid number of arguments to klee_set_time");

  if (!isa<ConstantExpr>(arguments[0])) {
    executor.terminateStateOnError(state, "klee_set_time requires a constant argument", "user.err");
    return;
  }

  state.stateTime = cast<ConstantExpr>(arguments[0])->getZExtValue();
}

void SpecialFunctionHandler::handleGetWList(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.empty() && "invalid number of arguments to klee_get_wlist");

  wlist_id_t id = state.getWaitingList();

  executor.bindLocal(target, state, ConstantExpr::create(id,
      executor.getWidthForLLVMType(executor.kmodule(state),
                                   target->inst->getType())));
}

void SpecialFunctionHandler::handleThreadPreempt(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 && "invalid number of arguments to klee_thread_preempt");

  if (!isa<ConstantExpr>(arguments[0])) {
    executor.terminateStateOnError(state, "klee_thread_preempt", "user.err");
  }

  executor.schedule(state, !arguments[0]->isZero());
}

void SpecialFunctionHandler::handleThreadSleep(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {

  assert(arguments.size() == 1 && "invalid number of arguments to klee_thread_sleep");

  ref<Expr> wlistExpr = executor.toUnique(state, arguments[0]);

  if (!isa<ConstantExpr>(wlistExpr)) {
    executor.terminateStateOnError(state, "klee_thread_sleep", "user.err");
    return;
  }

  state.sleepThread(cast<ConstantExpr>(wlistExpr)->getZExtValue());
  executor.schedule(state, false);
}

void SpecialFunctionHandler::handleThreadNotify(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 && "invalid number of arguments to klee_thread_notify");

  ref<Expr> wlist = executor.toUnique(state, arguments[0]);
  ref<Expr> all = executor.toUnique(state, arguments[1]);

  if (!isa<ConstantExpr>(wlist) || !isa<ConstantExpr>(all)) {
    executor.terminateStateOnError(state, "klee_thread_notify", "user.err");
    return;
  }

  if (all->isZero()) {
    executor.executeThreadNotifyOne(state, cast<ConstantExpr>(wlist)->getZExtValue());
  } else {
    // It's simple enough such that it can be handled by the state class itself
    state.notifyAll(cast<ConstantExpr>(wlist)->getZExtValue());
  }
}

void SpecialFunctionHandler::handleThreadBarrier(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {

  assert(arguments.size() == 3 && "invalid number of arguments to klee_thread_barrier");

  ref<Expr> barrierIdX = executor.toUnique(state, arguments[0]);
  ref<Expr> threadCountX = executor.toUnique(state, arguments[1]);
  ref<Expr> addrSpaceX = executor.toUnique(state, arguments[2]);

  if (!isa<ConstantExpr>(barrierIdX) || !isa<ConstantExpr>(threadCountX)
   || !isa<ConstantExpr>(addrSpaceX)) {
    executor.terminateStateOnError(state, "klee_thread_barrier", "user.err");
    return;
  }

  wlist_id_t barrierId = cast<ConstantExpr>(barrierIdX)->getZExtValue();
  unsigned threadCount = cast<ConstantExpr>(threadCountX)->getZExtValue();
  unsigned addrSpace = cast<ConstantExpr>(addrSpaceX)->getZExtValue();

  if (state.barrierThread(barrierId, threadCount, addrSpace))
    executor.schedule(state, false);
}

void SpecialFunctionHandler::handleThreadCreate(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 3 && "invalid number of arguments to klee_thread_create");

  ref<Expr> tid = executor.toUnique(state, arguments[0]);

  if (!isa<ConstantExpr>(tid)) {
    executor.terminateStateOnError(state, "klee_thread_create", "user.err");
    return;
  }

  executor.executeThreadCreate(state, cast<ConstantExpr>(tid)->getZExtValue(),
      arguments[1], arguments[2]);
}

void SpecialFunctionHandler::handleThreadTerminate(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.empty() && "invalid number of arguments to klee_thread_terminate");

  executor.executeThreadExit(state);
}

void SpecialFunctionHandler::handleBranch(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 && "invalid number of arguments to klee_branch");

  if (!isa<ConstantExpr>(arguments[1])) {
    executor.terminateStateOnError(state, "symbolic reason in klee_branch", "user.err");
    return;
  }

  // We must check that klee_branch is correctly used - the use case of the
  // return value must be a comparison instruction
  Instruction *inst = target->inst;

  if (!inst->hasOneUse()) {
    executor.terminateStateOnError(state, "klee_branch must be used once", "user.err");
    return;
  }

  User *user = *inst->use_begin();

  if (!isa<CmpInst>(user) || inst->getParent() != cast<Instruction>(user)->getParent()) {
    executor.terminateStateOnError(state, "klee_branch must be used together with a comparison", "user.err");
    return;
  }

  // We just bind the result to the first argument, and mark the reason

  state.crtForkReason = cast<ConstantExpr>(arguments[1])->getZExtValue();
  state.crtSpecialFork = cast<Instruction>(user);

  executor.bindLocal(target, state, arguments[0]);
}

void SpecialFunctionHandler::handleFork(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 && "invalid number of arguments to klee_fork");

  if (!isa<ConstantExpr>(arguments[0])) {
    executor.terminateStateOnError(state, "symbolic reason in klee_fork", "user.err");
    return;
  }

  int reason = cast<ConstantExpr>(arguments[0])->getZExtValue();

  executor.executeFork(state, target, reason);
}

void SpecialFunctionHandler::handleProcessFork(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 && "invalid number of arguments to klee_process_fork");

  ref<Expr> pid = executor.toUnique(state, arguments[0]);

  if (!isa<ConstantExpr>(pid)) {
    executor.terminateStateOnError(state, "klee_process_fork", "user.err");
    return;
  }

  executor.executeProcessFork(state, target,
      cast<ConstantExpr>(pid)->getZExtValue());
}

void SpecialFunctionHandler::handleProcessTerminate(ExecutionState &state,
                    KInstruction *target,
                    std::vector<ref<Expr> > &arguments) {
  assert(arguments.empty() && "invalid number of arguments to klee_process_terminate");

  executor.executeProcessExit(state);
}



void SpecialFunctionHandler::handleCheckMemoryAccess(ExecutionState &state,
                                                     KInstruction *target,
                                                     std::vector<ref<Expr> > 
                                                       &arguments) {
  assert(arguments.size()==2 &&
         "invalid number of arguments to klee_check_memory_access");

  ref<Expr> address = executor.toUnique(state, arguments[0]);
  ref<Expr> size = executor.toUnique(state, arguments[1]);
  if (!isa<ConstantExpr>(address) || !isa<ConstantExpr>(size)) {
    executor.terminateStateOnError(state, 
                                   "check_memory_access requires constant args",
                                   "user.err");
  } else {
    ObjectPair op;

    if (!state.addressSpace().resolveOne(cast<ConstantExpr>(address), op)) {
      executor.terminateStateOnError(state,
                                     "check_memory_access: memory error",
                                     "ptr.err",
                                     executor.getAddressInfo(state, address));
    } else {
      ref<Expr> chk = 
        op.first->getBoundsCheckPointer(address, 
                                        cast<ConstantExpr>(size)->getZExtValue());
      if (!chk->isTrue()) {
        executor.terminateStateOnError(state,
                                       "check_memory_access: memory error",
                                       "ptr.err",
                                       executor.getAddressInfo(state, address));
      }
    }
  }
}

void SpecialFunctionHandler::handleGetValue(ExecutionState &state,
                                            KInstruction *target,
                                            std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 &&
         "invalid number of arguments to klee_get_value");

  executor.executeGetValue(state, arguments[0], target);
}

void SpecialFunctionHandler::handleDefineFixedObject(ExecutionState &state,
                                                     KInstruction *target,
                                                     std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==2 &&
         "invalid number of arguments to klee_define_fixed_object");
  assert(isa<ConstantExpr>(arguments[0]) &&
         "expect constant address argument to klee_define_fixed_object");
  assert(isa<ConstantExpr>(arguments[1]) &&
         "expect constant size argument to klee_define_fixed_object");
  
  uint64_t address = cast<ConstantExpr>(arguments[0])->getZExtValue();
  uint64_t size = cast<ConstantExpr>(arguments[1])->getZExtValue();
  MemoryObject *mo = executor.memory->allocateFixed(address, size, state.prevPC()->inst);
  executor.bindObjectInState(state, 0, mo, false);
  mo->isUserSpecified = true; // XXX hack;
}

void SpecialFunctionHandler::handleMakeSymbolic(ExecutionState &state,
                                                KInstruction *target,
                                                std::vector<ref<Expr> > &arguments) {
  std::string name;

  // FIXME: For backwards compatibility, we should eventually enforce the
  // correct arguments.
  if (arguments.size() == 2) {
    name = "unnamed";
  } else {
    // FIXME: Should be a user.err, not an assert.
    assert(arguments.size()==3 &&
           "invalid number of arguments to klee_make_symbolic");  
    name = readStringAtAddress(state, arguments[2]);
  }

  resolutions_ty resList;

  processMemoryLocation(state, arguments[0], arguments[1], "make_symbolic", resList);

  for (resolutions_ty::iterator it = resList.begin(); it != resList.end();
      it++) {
    const MemoryObject *mo = it->first.first;
    const ObjectState *os = it->first.second;
    ExecutionState *s = it->second;

    mo->setName(name);

    if (os->readOnly) {
      executor.terminateStateOnError(*s,
                                     "cannot make readonly object symbolic",
                                     "user.err");
    } else {
      executor.executeMakeSymbolic(*s, mo, name, os->isShared);
    }
  }
}

void SpecialFunctionHandler::handleMarkGlobal(ExecutionState &state,
                                              KInstruction *target,
                                              std::vector<ref<Expr> > &arguments) {
  assert(arguments.size()==1 &&
         "invalid number of arguments to klee_mark_global");  

  Executor::ExactResolutionList rl;
  executor.resolveExact(state, arguments[0], rl, "mark_global");
  
  for (Executor::ExactResolutionList::iterator it = rl.begin(), 
         ie = rl.end(); it != ie; ++it) {
    const MemoryObject *mo = it->first.first;
    assert(!mo->isLocal);
    mo->isGlobal = true;
  }
}

void SpecialFunctionHandler::handleOclCompile(ExecutionState &state,
                                              KInstruction *target,
                                              std::vector<ref<Expr> > &arguments) {
#ifdef HAVE_OPENCL
  std::string codeName = "<OpenCL code>";

  std::string code = readStringAtAddress(state, arguments[0]);

  static unsigned codeNum = -1U;
  ++codeNum;

  if (DumpOpenCLSource) {
    char fileName[64];
    snprintf(fileName, 64, "cl%06d.cl", codeNum);
    codeName = executor.getHandler().getOutputFilename(fileName);

    llvm::OwningPtr<std::ostream> out(
      executor.getHandler().openOutputFile(fileName));
    *out << code;
  }

  MemoryBuffer *buf = MemoryBuffer::getMemBuffer(code);

  OwningPtr<clang::CompilerInvocation> CI(new clang::CompilerInvocation);

  clang::TextDiagnosticPrinter *DiagClient =
    new clang::TextDiagnosticPrinter(errs(), clang::DiagnosticOptions());
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR <= 8)
  clang::Diagnostic *Diag = new clang::Diagnostic(DiagClient);
#else
  IntrusiveRefCntPtr<clang::DiagnosticIDs> DiagID(new clang::DiagnosticIDs());
  clang::Diagnostic *Diag = new clang::Diagnostic(DiagID, DiagClient);
#endif

  SmallVector<StringRef, 8> splitArgs;
  ref<Expr> argsExpr = executor.toUnique(state, arguments[1]);
  if (ConstantExpr *argsCE = dyn_cast<ConstantExpr>(argsExpr)) {
    if (!argsCE->isZero()) {
      std::string args = readStringAtAddress(state, argsCE);
      SplitString(args, splitArgs);
    }
  }

  unsigned initialArgs = DumpOpenCLSource ? 3 : 2;

  const char **argv = new const char*[splitArgs.size() + initialArgs];
  argv[0] = "-x";
  argv[1] = "cl";
  if (DumpOpenCLSource)
    argv[2] = "-g";

       const char **argi = argv+initialArgs;
  for (SmallVector<StringRef, 8>::const_iterator
         i = splitArgs.begin(),
         e = splitArgs.end(); i != e; ++i, ++argi) {
    char *arg = new char[i->size() + 1];
    memcpy(arg, i->data(), i->size());
    arg[i->size()] = 0;
    *argi = arg;
  }

  clang::CompilerInvocation::CreateFromArgs(*CI, argv, argv+splitArgs.size()+initialArgs, *Diag);

  for (argi = argv+initialArgs; argi != argv+splitArgs.size()+initialArgs; ++argi) {
    delete *argi;
  }
  delete argv;

  CI->getHeaderSearchOpts().ResourceDir = LLVM_PREFIX "/lib/clang/" CLANG_VERSION_STRING;
  CI->getFrontendOpts().Inputs.clear();
  CI->getFrontendOpts().Inputs.push_back(std::pair<clang::InputKind, std::string>(
    clang::IK_OpenCL, codeName));
  CI->getPreprocessorOpts().Includes.push_back(KLEE_SRC_DIR "/include/klee/clkernel.h");
  CI->getPreprocessorOpts().addRemappedFile(codeName, buf);

  Triple kleeTriple(sys::getHostTriple());
  kleeTriple.setOS(Triple::KLEEOpenCL);
  CI->getTargetOpts().Triple = kleeTriple.str();

  clang::CompilerInstance Clang;
  Clang.setInvocation(CI.take());
  Clang.setDiagnostics(Diag);

  OwningPtr<clang::CodeGenAction> Act(new clang::EmitLLVMOnlyAction(&getGlobalContext()));
  if (!Clang.ExecuteAction(*Act)) {
    executor.bindLocal(target, state, 
                       ConstantExpr::create(0, sizeof(uintptr_t) * 8));
    return;
  }

  Module *Mod = Act->takeModule();
  if (DumpOpenCLModules) {
    char fileName[64];
    snprintf(fileName, 64, "cl%06d.ll", codeNum);

    llvm::OwningPtr<std::ostream> out(
      executor.getHandler().openOutputFile(fileName));
    llvm::raw_os_ostream rout(*out);
    Mod->print(rout, 0);
  }

  llvm::sys::Path LibraryDir(KLEE_DIR "/" RUNTIME_CONFIGURATION "/lib");
  llvm::sys::Path Path(LibraryDir);
  Path.appendComponent("libkleeRuntimeCLKernel.bca");
  Mod = klee::linkWithLibrary(Mod, Path.c_str());

  unsigned moduleId = executor.addModule(Mod, Interpreter::ModuleOptions(LibraryDir.c_str(), false, true));
  executor.initializeGlobals(state, moduleId);
  executor.bindModuleConstants(moduleId);

  executor.bindLocal(target, state, 
                     ConstantExpr::create((uintptr_t) Mod,
                                          sizeof(uintptr_t) * 8));
#else
  executor.terminateStateOnError(state, 
                                 "OpenCL support not available", 
                                 "opencl.err");
#endif
}

#ifdef HAVE_OPENCL
static cl_intern_arg_type typeAsCLArgType(const Type *t) {
  if (t->isIntegerTy(8))
    return CL_INTERN_ARG_TYPE_I8;
  if (t->isIntegerTy(16))
    return CL_INTERN_ARG_TYPE_I16;
  if (t->isIntegerTy(32))
    return CL_INTERN_ARG_TYPE_I32;
  if (t->isIntegerTy(64))
    return CL_INTERN_ARG_TYPE_I64;
  if (t->isFloatTy())
    return CL_INTERN_ARG_TYPE_F32;
  if (t->isDoubleTy())
    return CL_INTERN_ARG_TYPE_F64;
  if (const PointerType *pt = dyn_cast<PointerType>(t)) {
    if (pt->getAddressSpace() == 1)
      return CL_INTERN_ARG_TYPE_LOCAL_MEM;
    else
      return CL_INTERN_ARG_TYPE_MEM;
  }
  assert(0 && "Unknown type");
}
#endif

void SpecialFunctionHandler::handleOclGetArgType(ExecutionState &state,
                                                 KInstruction *target,
                                                 std::vector<ref<Expr> > &arguments) {
#ifdef HAVE_OPENCL
  uintptr_t function = cast<ConstantExpr>(arguments[0])->getZExtValue();
  Function *functionPtr = (Function *) function;

  size_t argIndex = cast<ConstantExpr>(arguments[1])->getZExtValue();
  const Type *argType = functionPtr->getFunctionType()->getParamType(argIndex);
  assert(argType && "arg out of bounds");
  cl_intern_arg_type argCLType = typeAsCLArgType(argType);

  executor.bindLocal(target, state, 
                     ConstantExpr::create(argCLType,
                                          sizeof(cl_intern_arg_type) * 8));
#else
  executor.terminateStateOnError(state, 
                                 "OpenCL support not available", 
                                 "opencl.err");
#endif
}

void SpecialFunctionHandler::handleOclGetArgCount(ExecutionState &state,
                                                  KInstruction *target,
                                                  std::vector<ref<Expr> > &arguments) {
  uintptr_t function = cast<ConstantExpr>(arguments[0])->getZExtValue();
  Function *functionPtr = (Function *) function;

  unsigned argCount = functionPtr->getFunctionType()->getNumParams();

  executor.bindLocal(target, state,
                     ConstantExpr::create(argCount,
                                          sizeof(unsigned) * 8));
}

void SpecialFunctionHandler::handleLookupModuleGlobal(ExecutionState &state,
                                                      KInstruction *target,
                                                      std::vector<ref<Expr> > &arguments) {
  uintptr_t module = cast<ConstantExpr>(arguments[0])->getZExtValue();
  std::string functionName = readStringAtAddress(state, arguments[1]);

  Module *modulePtr = (Module *) module;

  GlobalValue *globalVal = modulePtr->getNamedValue(functionName);
  if (!globalVal) {
    executor.bindLocal(target, state, 
                       ConstantExpr::create(0,
                                            sizeof(uintptr_t) * 8));
    return;
  }

  executor.bindLocal(target, state, executor.globalAddresses.find(globalVal)->second);
}

void SpecialFunctionHandler::handleICallCreateArgList(ExecutionState &state,
                                                      KInstruction *target,
                                                      std::vector<ref<Expr> > &arguments) {
  std::vector< ref<Expr> > *args = new std::vector< ref<Expr> >;

  executor.bindLocal(target, state, 
                     ConstantExpr::create((uintptr_t) args,
                                          sizeof(uintptr_t) * 8));
}

void SpecialFunctionHandler::handleICallAddArg(ExecutionState &state,
                                               KInstruction *target,
                                               std::vector<ref<Expr> > &arguments) {
  uintptr_t args = cast<ConstantExpr>(arguments[0])->getZExtValue();
  std::vector< ref<Expr> > *argsPtr = (std::vector< ref<Expr> > *) args;

  ref<Expr> argPtr = arguments[1];

  uintptr_t argSize = cast<ConstantExpr>(arguments[2])->getZExtValue();

  ObjectPair op;
  argPtr = executor.toUnique(state, argPtr);
  ref<ConstantExpr> address = cast<ConstantExpr>(argPtr);
  if (!state.addressSpace().resolveOne(address, op))
    assert(0 && "XXX out of bounds / multiple resolution unhandled");
  bool res;
  assert(executor.solver->mustBeTrue(state, 
                                     EqExpr::create(address, 
                                                    op.first->getBaseExpr()),
                                     res) &&
         res &&
         "XXX interior pointer unhandled");
  const ObjectState *os = op.second;

  ref<Expr> arg = os->read(0, argSize*8, state.crtThread().getTid());

  argsPtr->push_back(arg);
}

void SpecialFunctionHandler::handleICall(ExecutionState &state,
                                         KInstruction *target,
                                         std::vector<ref<Expr> > &arguments) {
  uintptr_t fn = cast<ConstantExpr>(arguments[0])->getZExtValue();
  Function *fnPtr = (Function *) fn;

  uintptr_t args = cast<ConstantExpr>(arguments[1])->getZExtValue();
  std::vector< ref<Expr> > *argsPtr = (std::vector< ref<Expr> > *) args;
  
  executor.executeCall(state, target, fnPtr, *argsPtr);
}

void SpecialFunctionHandler::handleICallDestroyArgList(ExecutionState &state,
                                                       KInstruction *target,
                                                       std::vector<ref<Expr> > &arguments) {
  uintptr_t args = cast<ConstantExpr>(arguments[0])->getZExtValue();
  std::vector< ref<Expr> > *argsPtr = (std::vector< ref<Expr> > *) args;

  delete argsPtr;
}

void SpecialFunctionHandler::handleCreateWorkGroup(ExecutionState &state,
                                                   KInstruction *target,
                                                   std::vector<ref<Expr> > &arguments) {
  unsigned workgroupId = state.wgAddressSpaces.size();

  AddressSpace *wgAddrSpace = new AddressSpace;

  wgAddrSpace->cowDomain = &state.wgCowDomain;
  state.wgCowDomain.push_back(wgAddrSpace);

  executor.bindGlobalsInNewAddressSpace(state, 1, *wgAddrSpace);
  state.wgAddressSpaces.push_back(wgAddrSpace);

  executor.bindLocal(target, state, 
                     ConstantExpr::create(workgroupId, sizeof(unsigned) * 8));
}

void SpecialFunctionHandler::handleSetWorkGroupId(ExecutionState &state,
                                                  KInstruction *target,
                                                  std::vector<ref<Expr> > &arguments) {
  unsigned workgroupId = cast<ConstantExpr>(arguments[0])->getZExtValue();

  state.crtThread().setWorkgroupId(workgroupId);
}

void SpecialFunctionHandler::handleSyscall(ExecutionState &state,
                            KInstruction *target,
                            std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() >= 1 && "invalid number of arguments to syscall");

  if (ConstantExpr *syscallNo = dyn_cast<ConstantExpr>(arguments[0])) {
    switch(syscallNo->getZExtValue()) {
    /* Signal syscalls */
    case SYS_rt_sigaction:
    case SYS_sigaltstack:
    case SYS_signalfd:
    case SYS_signalfd4:
    case SYS_rt_sigpending:
    case SYS_rt_sigprocmask:
    case SYS_rt_sigreturn:
    case SYS_rt_sigsuspend:
      executor.bindLocal(target, state, ConstantExpr::create(0,
            executor.getWidthForLLVMType(executor.kmodule(state),
                                         target->inst->getType())));
      break;
    default:
      executor.callUnmodelledFunction(state, target,
          executor.kmodule(state)->module->getFunction("syscall"),
          arguments);
      break;
    }
  } else {
    executor.terminateStateOnError(state, "syscall requires a concrete syscall number", "user.err");
  }
}

void SpecialFunctionHandler::handleSqrt(ExecutionState &state,
                                        KInstruction *target,
                                        std::vector<ref<Expr> > &arguments) {
  executor.bindLocal(target, state, 
                     FSqrtExpr::create(arguments[0], false));
}

void SpecialFunctionHandler::handleCos(ExecutionState &state,
                                        KInstruction *target,
                                        std::vector<ref<Expr> > &arguments) {
  executor.bindLocal(target, state, 
                     FCosExpr::create(arguments[0], false));
}

void SpecialFunctionHandler::handleSin(ExecutionState &state,
                                        KInstruction *target,
                                        std::vector<ref<Expr> > &arguments) {
  executor.bindLocal(target, state, 
                     FSinExpr::create(arguments[0], false));
}
