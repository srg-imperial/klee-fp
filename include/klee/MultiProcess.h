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
 * MultiProcess.h
 *
 *  Created on: Jul 22, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef MULTIPROCESS_H_
#define MULTIPROCESS_H_

#include "klee/Constraints.h"
#include "../../lib/Core/AddressSpace.h"
#include "klee/Threading.h"

#include <vector>
#include <set>
#include <map>

#include <stdint.h>

namespace klee {
class ExecutionState;
class Thread;

typedef uint64_t process_id_t;
typedef uint64_t thread_id_t;

class Process {
  friend class Thread;
  friend class ExecutionState;
  friend class Executor;
private:

  std::vector<unsigned int> forkPath; // 0 - parent, 1 - child

public:
  Process(process_id_t _pid, process_id_t _ppid) : pid(_pid), ppid(_ppid) { }

  process_id_t pid;
  process_id_t ppid;

  std::set<process_id_t> children;
  std::set<thread_uid_t> threads;

  AddressSpace addressSpace;
};
}


#endif /* MULTIPROCESS_H_ */
