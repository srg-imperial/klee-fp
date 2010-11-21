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
 * buffers.h
 *
 *  Created on: Aug 6, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef BUFFERS_H_
#define BUFFERS_H_

#include <unistd.h>

#include "common.h"
#include "multiprocess.h"

////////////////////////////////////////////////////////////////////////////////
// Event Queue Utility
////////////////////////////////////////////////////////////////////////////////

#define EVENT_READ  (1 << 0)
#define EVENT_WRITE (1 << 1)

typedef struct {
  wlist_id_t *queue;
  unsigned int count;
} event_queue_t;

void _event_queue_init(event_queue_t *q, unsigned count, int shared);
void _event_queue_finalize(event_queue_t *q);

int _event_queue_register(event_queue_t *q, wlist_id_t wlist);
int _event_queue_clear(event_queue_t *q, wlist_id_t wlist);

void _event_queue_notify(event_queue_t *q);

////////////////////////////////////////////////////////////////////////////////
// Stream Buffers
////////////////////////////////////////////////////////////////////////////////

#define BUFFER_STATUS_DESTROYING        (1<<0)
#define BUFFER_STATUS_CLOSED            (1<<1)
#define BUFFER_STATUS_SYM_READS         (1<<2)

// A basic producer-consumer data structure
typedef struct {
  char *contents;
  size_t max_size;
  size_t max_rsize;

  size_t start;
  size_t size;

  event_queue_t evt_queue;
  wlist_id_t empty_wlist;
  wlist_id_t full_wlist;

  unsigned int queued;
  unsigned short status;
} stream_buffer_t;

stream_buffer_t *_stream_create(size_t max_size, int shared);
void _stream_destroy(stream_buffer_t *buff);

ssize_t _stream_read(stream_buffer_t *buff, char *dest, size_t count);
ssize_t _stream_write(stream_buffer_t *buff, const char *src, size_t count);
void _stream_close(stream_buffer_t *buff);

int _stream_register_event(stream_buffer_t *buff, wlist_id_t wlist);
int _stream_clear_event(stream_buffer_t *buff, wlist_id_t wlist);

static inline int _stream_is_empty(stream_buffer_t *buff) {
  return (buff->size == 0);
}

static inline int _stream_is_full(stream_buffer_t *buff) {
  return (buff->size == buff->max_size);
}

static inline int _stream_is_closed(stream_buffer_t *buff) {
  return (buff->status & BUFFER_STATUS_CLOSED);
}

static inline void _stream_set_rsize(stream_buffer_t *buff, size_t rsize) {
  if (rsize == 0)
    rsize = 1;

  if (rsize > buff->max_size)
    rsize = buff->max_size;

  buff->max_rsize = rsize;
}

////////////////////////////////////////////////////////////////////////////////
// Block Buffers
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  char *contents;
  size_t max_size;
  size_t size;
} block_buffer_t;


void _block_init(block_buffer_t *buff, size_t max_size);
void _block_finalize(block_buffer_t *buff);
ssize_t _block_read(block_buffer_t *buff, char *dest, size_t count, size_t offset);
ssize_t _block_write(block_buffer_t *buff, const char *src, size_t count, size_t offset);


#endif /* BUFFERS_H_ */
