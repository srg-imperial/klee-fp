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
 * procsync.c
 *
 *  Created on: Sep 20, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#include "multiprocess.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <stdarg.h>

#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// IPC Semaphores
////////////////////////////////////////////////////////////////////////////////

// Taken from the manual page
union semun {
   int              val;    /* Value for SETVAL */
   struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
   unsigned short  *array;  /* Array for GETALL, SETALL */
   struct seminfo  *__buf;  /* Buffer for IPC_INFO
                               (Linux-specific) */
};

int semget(key_t key, int nsems, int semflg) {
  if (nsems < 0) {
    errno = EINVAL;
    return -1;
  }

  int index = MAX_SEMAPHORES;
  if (key != IPC_PRIVATE) {
    // Search for the key, if the set is already created
    for (index = 0; index < MAX_SEMAPHORES; index++) {
      if (STATIC_LIST_CHECK(__sems, (unsigned)index) &&
          __sems[index].descriptor.sem_perm.__key == key) {
        // This is it
        break;
      }
    }
  }

  if (index == MAX_SEMAPHORES) {
    if (!(semflg & IPC_CREAT)) {
      errno = ENOENT;
      return -1;
    }

    if (nsems == 0) {
      errno = EINVAL;
      return -1;
    }

    // Try to allocate a new semaphore set
    STATIC_LIST_ALLOC(__sems, index);

    if (index == MAX_SEMAPHORES) {
      errno = ENOSPC;
      return -1;
    }

    // At this point we know the index of the semaphore set
    sem_set_t *semset = &__sems[index];

    semset->sems = (sem_t*)malloc(nsems * sizeof(sem_t));
    memset(semset->sems, 0, nsems * sizeof(sem_t));
    klee_make_shared(semset->sems, nsems * sizeof(sem_t));

    semset->descriptor.sem_nsems = nsems;
    semset->descriptor.sem_ctime = time(0);
    semset->descriptor.sem_otime = 0;

    struct ipc_perm *perm = &semset->descriptor.sem_perm;
    perm->__key = key;
    perm->__seq = 0;
    perm->mode = semflg & 0777;
  } else {
    if ((semflg & (IPC_CREAT | IPC_EXCL)) == (IPC_CREAT | IPC_EXCL)) {
      errno = EEXIST;
      return -1;
    }
  }

  return index;
}

static void _sem_delete(int semid) {
  sem_set_t *semset = &__sems[semid];
  free(semset->sems);

  STATIC_LIST_CLEAR(__sems, semid);
}

static void _sem_setval(int semid, int semnum, unsigned short val) {
  sem_set_t *semset = &__sems[semid];

  semset->sems[semnum].semval = val;

  semset->descriptor.sem_ctime = time(0);
}

static void _sem_setall(int semid, unsigned short *vals) {
  sem_set_t *semset = &__sems[semid];

  unsigned i;
  for (i = 0; i < semset->descriptor.sem_nsems; i++) {
    semset->sems[i].semval = vals[i];
  }

  semset->descriptor.sem_ctime = time(0);
}

int semctl(int semid, int semnum, int cmd, ...) {
  if (!STATIC_LIST_CHECK(__sems, (unsigned)semid)) {
    errno = EINVAL;
    return -1;
  }

  union semun arg;
  va_list ap;

  // Get the argument
  va_start(ap, cmd);
  arg = va_arg(ap, union semun);
  va_end(ap);

  switch (cmd) {
  case IPC_RMID:
    _sem_delete(semid);
    return 0;
  case SETALL:
    _sem_setall(semid, arg.array);
    return 0;
  case SETVAL:
    _sem_setval(semid, semnum, arg.val);
    return 0;

  case IPC_STAT:
  case IPC_SET:
  case IPC_INFO:
  case SEM_INFO:
  case SEM_STAT:
  case GETALL:
  case GETNCNT:
  case GETPID:
  case GETVAL:
  case GETZCNT:
    klee_warning("unsupported semaphore operation");
    errno = EINVAL;
    return -1;
  default:
    // Invalid semaphore command
    errno = EINVAL;
    return -1;
  }
}

int semop(int semid, struct sembuf *sops, size_t nsops) {
  assert(0 && "not implemented");
}

int semtimedop(int semid, struct sembuf *sops, size_t nsops,
    const struct timespec *timeout) {
  assert(0 && "not implemented");
}
