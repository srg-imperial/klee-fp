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
 * threadsync.c
 *
 *  Created on: Aug 3, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#include "multiprocess.h"

#include <pthread.h>
#include <errno.h>
#include <klee/klee.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

////////////////////////////////////////////////////////////////////////////////
// POSIX Mutexes
////////////////////////////////////////////////////////////////////////////////

static void _mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
  mutex_data_t *mdata = (mutex_data_t*)malloc(sizeof(mutex_data_t));
  memset(mdata, 0, sizeof(mutex_data_t));

  *((mutex_data_t**)mutex) = mdata;

  mdata->wlist = klee_get_wlist();
  mdata->taken = 0;
}

static mutex_data_t *_get_mutex_data(pthread_mutex_t *mutex) {
  mutex_data_t *mdata = *((mutex_data_t**)mutex);

  if (mdata == STATIC_MUTEX_VALUE) {
    _mutex_init(mutex, 0);

    mdata = *((mutex_data_t**)mutex);
  }

  return mdata;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
  if (INJECT_FAULT(pthread_mutex_init, ENOMEM, EPERM)) {
    return -1;
  }

  _mutex_init(mutex, attr);

  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
  mutex_data_t *mdata = _get_mutex_data(mutex);

  free(mdata);

  return 0;
}

static int _atomic_mutex_lock(mutex_data_t *mdata, char try) {
  if (mdata->queued > 0 || mdata->taken) {
    if (try) {
      errno = EBUSY;
      return -1;
    } else {
      mdata->queued++;
      klee_thread_sleep(mdata->wlist);
      mdata->queued--;
    }
  }
  mdata->taken = 1;
  mdata->owner = pthread_self();

  return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
  mutex_data_t *mdata = _get_mutex_data(mutex);

  int res = _atomic_mutex_lock(mdata, 0);

  if (res == 0)
    klee_thread_preempt(0);

  return res;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
  mutex_data_t *mdata = _get_mutex_data(mutex);

  int res = _atomic_mutex_lock(mdata, 1);

  if (res == 0)
    klee_thread_preempt(0);

  return res;
}

static int _atomic_mutex_unlock(mutex_data_t *mdata) {
  if (!mdata->taken || mdata->owner != pthread_self()) {
    errno = EPERM;
    return -1;
  }

  mdata->taken = 0;

  if (mdata->queued > 0)
    klee_thread_notify_one(mdata->wlist);

  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
  mutex_data_t *mdata = _get_mutex_data(mutex);

  int res = _atomic_mutex_unlock(mdata);

  klee_thread_preempt(0);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
// POSIX Condition Variables
////////////////////////////////////////////////////////////////////////////////

static void _cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
  condvar_data_t *cdata = (condvar_data_t*)malloc(sizeof(condvar_data_t));
  memset(cdata, 0, sizeof(condvar_data_t));

  *((condvar_data_t**)cond) = cdata;

  cdata->wlist = klee_get_wlist();
}

static condvar_data_t *_get_condvar_data(pthread_cond_t *cond) {
  condvar_data_t *cdata = *((condvar_data_t**)cond);

  if (cdata == STATIC_CVAR_VALUE) {
    _cond_init(cond, 0);

    cdata = *((condvar_data_t**)cond);
  }

  return cdata;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
  if (INJECT_FAULT(pthread_cond_init, ENOMEM, EAGAIN)) {
    return -1;
  }

  _cond_init(cond, attr);

  return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
  condvar_data_t *cdata = _get_condvar_data(cond);

  free(cdata);

  return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
    const struct timespec *abstime) {
  assert(0 && "not implemented");
  return -1;
}

static int _atomic_cond_wait(condvar_data_t *cdata, mutex_data_t *mdata) {
  if (cdata->queued > 0) {
    if (cdata->mutex != mdata) {
      errno = EINVAL;
      return -1;
    }
  } else {
    cdata->mutex = mdata;
  }

  if (_atomic_mutex_unlock(mdata) != 0) {
    errno = EPERM;
    return -1;
  }

  cdata->queued++;
  klee_thread_sleep(cdata->wlist);
  cdata->queued--;

  if (_atomic_mutex_lock(mdata, 0) != 0) {
    errno = EPERM;
    return -1;
  }

  return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
  condvar_data_t *cdata = _get_condvar_data(cond);
  mutex_data_t *mdata = _get_mutex_data(mutex);

  int res = _atomic_cond_wait(cdata, mdata);

  if (res == 0)
    klee_thread_preempt(0);

  return res;
}

static int _atomic_cond_notify(condvar_data_t *cdata, char all) {
  if (cdata->queued > 0) {
    if (all)
      klee_thread_notify_all(cdata->wlist);
    else
      klee_thread_notify_one(cdata->wlist);
  }

  return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
  condvar_data_t *cdata = _get_condvar_data(cond);

  int res = _atomic_cond_notify(cdata, 1);

  if (res == 0)
    klee_thread_preempt(0);

  return res;
}

int pthread_cond_signal(pthread_cond_t *cond) {
  condvar_data_t *cdata = _get_condvar_data(cond);

  int res = _atomic_cond_notify(cdata, 0);

  if (res == 0)
    klee_thread_preempt(0);

  return res;
}
