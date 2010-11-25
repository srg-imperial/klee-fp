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
 * models.c
 *
 *  Created on: Aug 10, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#define __FORCE_USE_MODELS
#include "models.h"

void _exit(int status);
void pthread_exit(void *value_ptr);

FORCE_IMPORT(_exit);
FORCE_IMPORT(pthread_exit);

__attribute__((used)) void __force_model_linkage(void) {
  // Just do nothing
}
