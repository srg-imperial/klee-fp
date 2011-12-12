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
 * ForkTag.h
 *
 *  Created on: Sep 4, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef FORKTAG_H_
#define FORKTAG_H_

namespace klee {

struct KFunction;

enum ForkClass {
  KLEE_FORK_DEFAULT = 0,
  KLEE_FORK_FAULTINJ = 1,
  KLEE_FORK_SCHEDULE = 2,
  KLEE_FORK_INTERNAL = 3,
  KLEE_FORK_MULTI = 4,
  KLEE_FORK_TIMEOUT = 5
};

struct ForkTag {
  ForkClass forkClass;

  // For fault injection
  bool fiVulnerable;

  // The location in the code where the fork was decided (it can be NULL)
  KFunction *location;

  ForkTag(ForkClass _fclass) :
    forkClass(_fclass), fiVulnerable(false), location(0) { }
};

}


#endif /* FORKTAG_H_ */
