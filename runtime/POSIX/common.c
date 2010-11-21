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
 * common.c
 *
 *  Created on: Aug 11, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#include "common.h"

#include <sys/types.h>
#include <errno.h>
#include <assert.h>

#include <klee/klee.h>

void *__concretize_ptr(const void *p) {
  char *pc = (char*) klee_get_valuel((long) p);
  klee_assume(pc == p);
  return pc;
}

size_t __concretize_size(size_t s) {
  size_t sc = klee_get_valuel((long)s);
  klee_assume(sc == s);
  return sc;
}

const char *__concretize_string(const char *s) {
  char *sc = __concretize_ptr(s);
  unsigned i;

  for (i=0; ; ++i) {
    char c = *sc;
    if (!(i&(i-1))) {
      if (!c) {
        *sc++ = 0;
        break;
      } else if (c=='/') {
        *sc++ = '/';
      }
    } else {
      char cc = (char) klee_get_valuel((long)c);
      klee_assume(cc == c);
      *sc++ = cc;
      if (!cc) break;
    }
  }

  return s;
}

int __inject_fault(const char *fname, int eno, ...) {
  if (!klee_fork(__KLEE_FORK_FAULTINJ)) {
    return 0;
  }

  errno = eno;
  return 1;
}

unsigned __fork_values(unsigned min, unsigned max, int reason) {
  assert(max >= min);

  unsigned i;
  for (i = min; i < max;) {
    if (!klee_fork(reason)) {
      return i;
    }

    if (i == 0)
      i++;
    else
      i <<= 1;
  }

  return max;
}
