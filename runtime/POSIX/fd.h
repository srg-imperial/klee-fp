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
 * fd.h
 *
 *  Created on: Aug 6, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef FD_H_
#define FD_H_

#include "common.h"

#include <sys/uio.h>

#define FD_IS_FILE          (1 << 3)    // The fd points to a disk file
#define FD_IS_SOCKET        (1 << 4)    // The fd points to a socket
#define FD_IS_PIPE          (1 << 5)    // The fd points to a pipe
#define FD_CLOSE_ON_EXEC    (1 << 6)    // The fd should be closed at exec() time (ignored)

typedef struct {
  unsigned int refcount;
  unsigned int queued;
  int flags;
} file_base_t;

typedef struct {
  unsigned int attr;

  file_base_t *io_object;

  char allocated;
} fd_entry_t;

extern fd_entry_t __fdt[MAX_FDS];

void klee_init_fds(unsigned n_files, unsigned file_length,
                   int sym_stdout_flag);

void __adjust_fds_on_fork(void);
void __close_fds(void);

#define _FD_SET(n, p)    ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define _FD_CLR(n, p)    ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define _FD_ISSET(n, p)  ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define _FD_ZERO(p)  memset((char *)(p), '\0', sizeof(*(p)))

ssize_t _scatter_read(int fd, const struct iovec *iov, int iovcnt);
ssize_t _gather_write(int fd, const struct iovec *iov, int iovcnt);


#endif /* FD_H_ */
