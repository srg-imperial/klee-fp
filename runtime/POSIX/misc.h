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
 * misc.h
 *
 *  Created on: Sep 20, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef MISC_H_
#define MISC_H_

#include "common.h"

#include <stddef.h>

typedef struct {
  void *addr;
  size_t length;

  int prot;
  int flags;

  char allocated;
} mmap_block_t;

extern mmap_block_t __mmaps[MAX_MMAPS];

void klee_init_mmap(void);


#endif /* MISC_H_ */
