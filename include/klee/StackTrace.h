/*******************************************************************************
 * Copyright (C) 2010 Dependable Systems Laboratory, EPFL
 *
 * This file is part of the Cloud9-specific extensions to the KLEE symbolic
 * execution engine.
 *
 * Do NOT distribute this file to any third party; it is part of
 * unpublished work.
 *
 *******************************************************************************
 * StackTrace.h
 *
 *  Created on: Oct 1, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef STACKTRACE_H_
#define STACKTRACE_H_

#include "klee/Expr.h"

#include <vector>
#include <iostream>

namespace klee {

class KFunction;
class KInstruction;

class StackTrace {
  friend class ExecutionState;
  friend class Thread;
private:
  typedef std::pair<KFunction*, const KInstruction*> position_t;

  typedef std::pair<position_t, std::vector<ref<Expr> > > frame_t;

  typedef std::vector<frame_t> stack_t;

  stack_t contents;

  StackTrace() { } // Cannot construct directly
public:
  void dump(std::ostream &out);
};

}


#endif /* STACKTRACE_H_ */
