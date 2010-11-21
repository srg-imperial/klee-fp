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
 * buffers.c
 *
 *  Created on: Aug 6, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#include "buffers.h"

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// Event Queue Utility
////////////////////////////////////////////////////////////////////////////////

void _event_queue_init(event_queue_t *q, unsigned count, int shared) {
  q->queue = (wlist_id_t*)malloc(count * sizeof(wlist_id_t));

  memset(q->queue, 0, count * sizeof(wlist_id_t));

  if (shared)
    klee_make_shared(q->queue, count * sizeof(wlist_id_t));

  q->count = count;
}

void _event_queue_finalize(event_queue_t *q) {
  free(q->queue);
}

int _event_queue_register(event_queue_t *q, wlist_id_t wlist) {
  unsigned i;
  for (i = 0; i < q->count; i++) {
    if (q->queue[i] == 0) {
      q->queue[i] = wlist;
      return 0;
    }
  }

  return -1;
}

int _event_queue_clear(event_queue_t *q, wlist_id_t wlist) {
  unsigned i;
  for (i = 0; i < q->count; i++) {
    if (q->queue[i] == wlist) {
      q->queue[i] = 0;
      return 0;
    }
  }

  return -1;
}

void _event_queue_notify(event_queue_t *q) {
  unsigned i;
  for (i = 0; i < q->count; i++) {
    if (q->queue[i] > 0)
      klee_thread_notify_all(q->queue[i]);
  }

  memset(q->queue, 0, q->count*sizeof(wlist_id_t));
}

////////////////////////////////////////////////////////////////////////////////
// Stream Buffers
////////////////////////////////////////////////////////////////////////////////

void __notify_event(stream_buffer_t *buff, char event) {
  if (event & EVENT_READ)
    klee_thread_notify_all(buff->empty_wlist);

  if (event & EVENT_WRITE)
    klee_thread_notify_all(buff->full_wlist);

  _event_queue_notify(&buff->evt_queue);
}

stream_buffer_t *_stream_create(size_t max_size, int shared) {
  stream_buffer_t *buff = (stream_buffer_t*)malloc(sizeof(stream_buffer_t));

  memset(buff, 0, sizeof(stream_buffer_t));
  buff->contents = (char*) malloc(max_size);
  buff->max_size = max_size;
  buff->max_rsize = max_size;
  buff->queued = 0;
  buff->empty_wlist = klee_get_wlist();
  buff->full_wlist = klee_get_wlist();
  _event_queue_init(&buff->evt_queue, MAX_EVENTS, shared);

  if (shared) {
    klee_make_shared(buff, sizeof(stream_buffer_t));
    klee_make_shared(buff->contents, max_size);
  }

  return buff;
}

void _stream_destroy(stream_buffer_t *buff) {
  __notify_event(buff, EVENT_READ | EVENT_WRITE);

  free(buff->contents);
  _event_queue_finalize(&buff->evt_queue);

  if (buff->queued == 0) {
    free(buff);
  } else {
    buff->status |= BUFFER_STATUS_DESTROYING;
  }
}

void _stream_close(stream_buffer_t *buff) {
  __notify_event(buff, EVENT_READ | EVENT_WRITE);

  buff->status |= BUFFER_STATUS_CLOSED;
}

ssize_t _stream_read(stream_buffer_t *buff, char *dest, size_t count) {
  if (count == 0) {
    return 0;
  }

  while (buff->size == 0) {
    if (buff->status & BUFFER_STATUS_CLOSED) {
      return 0;
    }

    buff->queued++;
    klee_thread_sleep(buff->empty_wlist);
    buff->queued--;

    if (buff->status & BUFFER_STATUS_DESTROYING) {
      if (buff->queued == 0)
        free(buff);

      return -1;
    }
  }

  if (buff->size < count)
    count = buff->size;
  if (buff->max_rsize < count)
    count = buff->max_rsize;

  if (buff->status & BUFFER_STATUS_SYM_READS) {
    count = __fork_values(1, count, __KLEE_FORK_DEFAULT);
  }

  if (buff->start + count > buff->max_size) {
    size_t overflow = (buff->start + count) % buff->max_size;

    memcpy(dest, &buff->contents[buff->start], count - overflow);
    memcpy(&dest[count-overflow], &buff->contents[0], overflow);
  } else {
    memcpy(dest, &buff->contents[buff->start], count);
  }

  buff->start = (buff->start + count) % buff->max_size;
  buff->size -= count;

  __notify_event(buff, EVENT_WRITE);

  return count;
}

ssize_t _stream_write(stream_buffer_t *buff, const char *src, size_t count) {
  if (count == 0) {
    return 0;
  }

  if (buff->status & BUFFER_STATUS_CLOSED) {
    return 0;
  }

  while (buff->size == buff->max_size) {
    buff->queued++;
    klee_thread_sleep(buff->full_wlist);
    buff->queued--;

    if (buff->status & BUFFER_STATUS_DESTROYING) {
      if (buff->queued == 0)
        free(buff);

      return -1;
    }

    if (buff->status & BUFFER_STATUS_CLOSED) {
      return 0;
    }
  }

  if (count > buff->max_size - buff->size)
    count = buff->max_size - buff->size;

  size_t end = (buff->start + buff->size) % buff->max_size;

  if (end + count > buff->max_size) {
    size_t overflow = (end + count) % buff->max_size;

    memcpy(&buff->contents[end], src, count - overflow);
    memcpy(&buff->contents[0], &src[count - overflow], overflow);
  } else {
    memcpy(&buff->contents[end], src, count);
  }

  buff->size += count;

  __notify_event(buff, EVENT_READ);

  return count;
}

int _stream_register_event(stream_buffer_t *buff, wlist_id_t wlist) {
  return _event_queue_register(&buff->evt_queue, wlist);
}

int _stream_clear_event(stream_buffer_t *buff, wlist_id_t wlist) {
  return _event_queue_clear(&buff->evt_queue, wlist);
}

////////////////////////////////////////////////////////////////////////////////
// Block Buffers
////////////////////////////////////////////////////////////////////////////////

void _block_init(block_buffer_t *buff, size_t max_size) {
  memset(buff, 0, sizeof(block_buffer_t));
  buff->contents = (char*)malloc(max_size);
  buff->max_size = max_size;
  buff->size = 0;
}

void _block_finalize(block_buffer_t *buff) {
  free(buff->contents);
}

ssize_t _block_read(block_buffer_t *buff, char *dest, size_t count, size_t offset) {
  if (offset > buff->size)
    return -1;

  if (offset + count > buff->size)
    count = buff->size - offset;

  if (count == 0)
    return 0;

  memcpy(dest, &buff->contents[offset], count);

  return count;
}

ssize_t _block_write(block_buffer_t *buff, const char *src, size_t count, size_t offset) {
  if (offset > buff->max_size)
    return -1;

  if (offset + count > buff->max_size)
    count = buff->max_size - offset;

  if (count == 0)
    return 0;

  buff->size = offset + count;

  memcpy(&buff->contents[offset], src, count);

  return count;
}
