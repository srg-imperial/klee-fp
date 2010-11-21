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
 * multiprocess.h
 *
 *  Created on: Jul 31, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef THREADS_H_
#define THREADS_H_

#include <stdint.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "common.h"

typedef uint64_t wlist_id_t;

#define DEFAULT_THREAD  0

#define DEFAULT_PROCESS 2
#define DEFAULT_PARENT  1
#define DEFAULT_UMASK   (S_IWGRP | S_IWOTH)

#define PID_TO_INDEX(pid)   ((pid) - 2)
#define INDEX_TO_PID(idx)   ((idx) + 2)

#define STATIC_MUTEX_VALUE      0
#define STATIC_CVAR_VALUE       0

////////////////////////////////////////////////////////////////////////////////
// System Wide Data Structures
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  wlist_id_t wlist;
  wlist_id_t children_wlist;

  pid_t parent;
  mode_t umask;

  int ret_value;

  char allocated;
  char terminated;
} proc_data_t;

extern proc_data_t __pdata[MAX_PROCESSES];

typedef struct {
  // Taken from the semaphore specs
  unsigned short semval;
  unsigned short semzcnt;
  unsigned short semncnt;
  pid_t sempid;
} sem_t;

typedef struct {
  struct semid_ds descriptor;
  sem_t *sems;

  char allocated;
} sem_set_t;

extern sem_set_t __sems[MAX_SEMAPHORES];



////////////////////////////////////////////////////////////////////////////////
// Process Specific Data Structures
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  wlist_id_t wlist;

  void *ret_value;

  char allocated;
  char terminated;
  char joinable;
} thread_data_t;

typedef struct {
  wlist_id_t wlist;

  char taken;
  unsigned int owner;
  unsigned int queued;

  char allocated;
} mutex_data_t;

typedef struct {
  wlist_id_t wlist;

  mutex_data_t *mutex;
  unsigned int queued;
} condvar_data_t;

typedef struct {
  thread_data_t threads[MAX_THREADS];
} tsync_data_t;

extern tsync_data_t __tsync;

void klee_init_processes(void);
void klee_init_threads(void);


#endif /* THREADS_H_ */
