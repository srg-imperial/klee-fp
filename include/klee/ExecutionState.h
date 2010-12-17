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

//===-- ExecutionState.h ----------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_EXECUTIONSTATE_H
#define KLEE_EXECUTIONSTATE_H

#include "klee/Constraints.h"
#include "klee/Expr.h"
#include "klee/Internal/ADT/TreeStream.h"

// FIXME: We do not want to be exposing these? :(
#include "../../lib/Core/AddressSpace.h"
#include "klee/Internal/Module/KInstIterator.h"

#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 9)
#include "llvm/System/TimeValue.h"
#else
#include "llvm/Support/TimeValue.h"
#endif

#include "klee/Threading.h"
#include "klee/MultiProcess.h"
#include "klee/AddressPool.h"
#include "klee/StackTrace.h"

#include <map>
#include <set>
#include <vector>

using namespace llvm;

namespace llvm {
class Instruction;
}

namespace klee {
  class Array;
  class CallPathNode;
  struct Cell;
  struct KFunction;
  struct KInstruction;
  class MemoryObject;
  class PTreeNode;
  struct InstructionInfo;

std::ostream &operator<<(std::ostream &os, const MemoryMap &mm);

typedef uint64_t wlist_id_t;

class ExecutionState {
	friend class ObjectState;

public:
  typedef std::vector<StackFrame> stack_ty;

  typedef std::map<thread_uid_t, Thread> threads_ty;
  typedef std::map<process_id_t, Process> processes_ty;
  typedef std::map<wlist_id_t, std::set<thread_uid_t> > wlists_ty;

  typedef std::vector<AddressSpace *>::iterator wg_addrspace_iterator;

private:
  // unsupported, use copy constructor
  ExecutionState &operator=(const ExecutionState&); 
  std::map< std::string, std::string > fnAliases;

  void setupMain(KFunction *kf, unsigned moduleId);
  void setupTime();
  void setupAddressPool();
public:

  bool fakeState;
  // Are we currently underconstrained?  Hack: value is size to make fake
  // objects.
  unsigned depth;

  /// Disables forking, set by user code.
  bool forkDisabled;


  mutable double queryCost;
  double weight;

  TreeOStream pathOS, symPathOS;
  unsigned instsSinceCovNew;
  bool coveredNew;

  std::map<const std::string*, std::set<unsigned> > coveredLines;

  PTreeNode *ptreeNode;

  int crtForkReason;
  Instruction *crtSpecialFork;

  std::vector<AddressSpace *> wgAddressSpaces;

  /// ordered list of symbolics: used to generate test cases. 
  //
  // FIXME: Move to a shared list structure (not critical).
  std::vector< std::pair<const MemoryObject*, const Array*> > symbolics;

  ConstraintManager globalConstraints;


  // For a multi threaded ExecutionState
  threads_ty threads;
  processes_ty processes;

  wlists_ty waitingLists;
  wlist_id_t wlistCounter;

  uint64_t stateTime;

  AddressPool addressPool;
  AddressSpace::cow_domain_t cowDomain, thrCowDomain, wgCowDomain;

  Thread& createThread(thread_id_t tid, KFunction *kf, unsigned moduleId);
  Process& forkProcess(process_id_t pid);
  void terminateThread(threads_ty::iterator it);
  void terminateProcess(processes_ty::iterator it);

  threads_ty::iterator nextThread(threads_ty::iterator it) {
    if (it == threads.end())
      it = threads.begin();
    else {
      it++;
      if (it == threads.end())
        it = threads.begin();
    }

    crtProcessIt = processes.find(crtThreadIt->second.getPid());

    return it;
  }

  void scheduleNext(threads_ty::iterator it) {
    //CLOUD9_DEBUG("New thread scheduled: " << it->second.tid << " (pid: " << it->second.pid << ")");
    assert(it != threads.end());

    crtThreadIt = it;
    crtProcessIt = processes.find(crtThreadIt->second.getPid());
  }

  wlist_id_t getWaitingList() { return wlistCounter++; }
  void sleepThread(wlist_id_t wlist);
  void notifyOne(wlist_id_t wlist, thread_uid_t tid);
  void notifyAll(wlist_id_t wlist);

  threads_ty::iterator crtThreadIt;
  processes_ty::iterator crtProcessIt;

  unsigned int preemptions;


  /* Shortcut methods */

  Thread &crtThread() { return crtThreadIt->second; }
  const Thread &crtThread() const { return crtThreadIt->second; }

  Process &crtProcess() { return crtProcessIt->second; }
  const Process &crtProcess() const { return crtProcessIt->second; }

  ConstraintManager &constraints() { return globalConstraints; }
  const ConstraintManager &constraints() const { return globalConstraints; }

  AddressSpace &addressSpace(unsigned addrspace = 0);
  const AddressSpace &addressSpace(unsigned addrspace = 0) const {
    return const_cast<ExecutionState *>(this)->addressSpace(addrspace);
  }

  KInstIterator& pc() { return crtThread().pc; }
  const KInstIterator& pc() const { return crtThread().pc; }

  KInstIterator& prevPC() { return crtThread().prevPC; }
  const KInstIterator& prevPC() const { return crtThread().prevPC; }

  stack_ty& stack() { return crtThread().stack; }
  const stack_ty& stack() const { return crtThread().stack; }

  // Current memory watchpoint.
  ref<Expr> watchpoint;
  size_t watchpointSize;

  std::string getFnAlias(std::string fn);
  void addFnAlias(std::string old_fn, std::string new_fn);
  void removeFnAlias(std::string fn);

public:
  ExecutionState(KFunction *kf, unsigned moduleId);

  // XXX total hack, just used to make a state so solver can
  // use on structure
  ExecutionState(const std::vector<ref<Expr> > &assumptions);
  ExecutionState(const ExecutionState &that);

  ~ExecutionState();
  
  ExecutionState *branch();

  void pushFrame(Thread &t, KInstIterator caller, KFunction *kf, unsigned moduleId) {
    t.stack.push_back(StackFrame(caller,kf,moduleId));
  }
  void pushFrame(KInstIterator caller, KFunction *kf, unsigned moduleId) {
    pushFrame(crtThread(), caller, kf, moduleId);
  }

  void popFrame(Thread &t) {
    StackFrame &sf = t.stack.back();
    for (std::vector<const MemoryObject*>::iterator it = sf.allocas.begin(),
           ie = sf.allocas.end(); it != ie; ++it)
      processes.find(t.getPid())->second.addressSpace.unbindObject(*it);
    t.stack.pop_back();
  }
  void popFrame() {
    popFrame(crtThread());
  }

  void addSymbolic(const MemoryObject *mo, const Array *array) { 
    symbolics.push_back(std::make_pair(mo, array));
  }
  void addConstraint(ref<Expr> e) { 
    constraints().addConstraint(e);
  }

  bool merge(const ExecutionState &b);

  StackTrace getStackTrace() const;

  wg_addrspace_iterator wg_addrspace_begin() {
    return wgAddressSpaces.begin();
  }

  wg_addrspace_iterator wg_addrspace_end() {
    return wgAddressSpaces.end();
  }

};

}

#endif
