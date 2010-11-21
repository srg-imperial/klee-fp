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
 * pipes.h
 *
 *  Created on: Aug 7, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef PIPES_H_
#define PIPES_H_

#include "fd.h"

#include "buffers.h"

typedef struct {
  file_base_t __bdata;

  stream_buffer_t *buffer;
} pipe_end_t;

int _close_pipe(pipe_end_t *pipe);
ssize_t _read_pipe(pipe_end_t *pipe, void *buf, size_t count);
ssize_t _write_pipe(pipe_end_t *pipe, const void *buf, size_t count);
int _stat_pipe(pipe_end_t *pipe, struct stat *buf);

int _is_blocking_pipe(pipe_end_t *pipe, int event);
int _register_events_pipe(pipe_end_t *pipe, wlist_id_t wlist, int events);
void _deregister_events_pipe(pipe_end_t *pipe, wlist_id_t wlist, int events);

#endif /* PIPES_H_ */
