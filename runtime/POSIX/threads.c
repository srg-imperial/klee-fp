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
 * threads.c
 *
 *  Created on: Jul 29, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#include "multiprocess.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// The PThreads API
////////////////////////////////////////////////////////////////////////////////

pthread_t pthread_self(void) {
  pthread_t result;

  klee_get_context(&result, 0);

  return result;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
    void *(*start_routine)(void*), void *arg) {

  if (INJECT_FAULT(pthread_create, EAGAIN)) {
    return -1;
  }

  unsigned int newIdx;
  STATIC_LIST_ALLOC(__tsync.threads, newIdx);

  if (newIdx == MAX_THREADS) {
    errno = EAGAIN;
    return -1;
  }

  thread_data_t *tdata = &__tsync.threads[newIdx];
  tdata->terminated = 0;
  tdata->joinable = 1; // TODO: Read this from an attribute
  tdata->wlist = klee_get_wlist();

  klee_thread_create(newIdx, start_routine, arg);

  *thread = newIdx;

  return 0;
}

void pthread_exit(void *value_ptr) {
  unsigned int idx = pthread_self();
  thread_data_t *tdata = &__tsync.threads[idx];

  if (tdata->joinable) {
    tdata->terminated = 1;
    tdata->ret_value = value_ptr;

    klee_thread_notify_all(tdata->wlist);
  } else {
    STATIC_LIST_CLEAR(__tsync.threads, idx);
  }

  klee_thread_terminate(); // Does not return
}


int pthread_join(pthread_t thread, void **value_ptr) {
  if (thread >= MAX_THREADS) {
    errno = ESRCH;
    return -1;
  }

  if (thread == pthread_self()) {
    errno = EDEADLK;
    return -1;
  }

  thread_data_t *tdata = &__tsync.threads[thread];

  if (!tdata->allocated) {
    errno = ESRCH;
    return -1;
  }

  if (!tdata->joinable) {
    errno = EINVAL;
    return -1;
  }

  if (!tdata->terminated)
    klee_thread_sleep(tdata->wlist);

  if (value_ptr) {
    *value_ptr = tdata->ret_value;
  }

  STATIC_LIST_CLEAR(__tsync.threads, thread);

  return 0;
}

int pthread_detach(pthread_t thread) {
  if (thread >= MAX_THREADS) {
    errno = ESRCH;
  }

  thread_data_t *tdata = &__tsync.threads[thread];

  if (!tdata->allocated) {
    errno = ESRCH;
    return -1;
  }

  if (!tdata->joinable) {
    errno = EINVAL;
    return -1;
  }

  if (tdata->terminated) {
    STATIC_LIST_CLEAR(__tsync.threads, thread);
  } else {
    tdata->joinable = 0;
  }

  return 0;
}

int pthread_attr_init(pthread_attr_t *attr) {
  klee_warning("pthread_attr_init does nothing");
  return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr) {
  klee_warning("pthread_attr_destroy does nothing");
  return 0;
}
