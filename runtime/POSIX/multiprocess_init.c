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
 * multiprocess_init.c
 *
 *  Created on: Aug 2, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#include "multiprocess.h"

#include "models.h"

#include <string.h>
#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// Processes
////////////////////////////////////////////////////////////////////////////////

proc_data_t __pdata[MAX_PROCESSES];
sem_set_t __sems[MAX_SEMAPHORES];

static void klee_init_semaphores(void) {
  STATIC_LIST_INIT(__sems);
  klee_make_shared(__sems, sizeof(__sems));
}

void klee_init_processes(void) {
  STATIC_LIST_INIT(__pdata);
  klee_make_shared(__pdata, sizeof(__pdata));

  proc_data_t *pdata = &__pdata[PID_TO_INDEX(DEFAULT_PROCESS)];
  pdata->allocated = 1;
  pdata->terminated = 0;
  pdata->parent = DEFAULT_PARENT;
  pdata->umask = DEFAULT_UMASK;
  pdata->wlist = klee_get_wlist();
  pdata->children_wlist = klee_get_wlist();

  klee_init_semaphores();

  klee_init_threads();
}

////////////////////////////////////////////////////////////////////////////////
// Threads
////////////////////////////////////////////////////////////////////////////////

tsync_data_t __tsync;

void klee_init_threads(void) {
  STATIC_LIST_INIT(__tsync.threads);

  // Thread initialization
  thread_data_t *def_data = &__tsync.threads[DEFAULT_THREAD];
  def_data->allocated = 1;
  def_data->terminated = 0;
  def_data->ret_value = 0;
  def_data->joinable = 1; // Why not?
  def_data->wlist = klee_get_wlist();
}
