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
 * fd_init.c
 *
 *  Created on: Aug 7, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#include "fd.h"
#include "files.h"
#include "sockets.h"

#include "models.h"

#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <klee/klee.h>

// File descriptor table
fd_entry_t __fdt[MAX_FDS];

// Symbolic file system
filesystem_t    __fs;
disk_file_t __stdin_file;

// Symbolic network
network_t       __net;
unix_t          __unix_net;

static void _init_fdt(void) {
  STATIC_LIST_INIT(__fdt);

  int fd;

  fd = _open_symbolic(&__stdin_file, O_RDONLY, 0);
  assert(fd == 0);

  fd = _open_concrete(1, O_WRONLY);
  assert(fd == 1);

  fd = _open_concrete(1, O_WRONLY);
  assert(fd == 2);
}

static void _init_symfiles(unsigned n_files, unsigned file_length) {
  char fname[] = "FILE??";
  unsigned int fname_len = strlen(fname);

  struct stat s;
  int res = CALL_UNDERLYING(stat, ".", &s);

  assert(res == 0 && "Could not get default stat values");

  klee_make_shared(&__fs, sizeof(filesystem_t));
  memset(&__fs, 0, sizeof(filesystem_t));

  __fs.count = n_files;
  __fs.files = (disk_file_t**)malloc(n_files*sizeof(disk_file_t*));
  klee_make_shared(__fs.files, n_files*sizeof(disk_file_t*));

  // Create n symbolic files
  unsigned int i;
  for (i = 0; i < n_files; i++) {
    __fs.files[i] = (disk_file_t*)malloc(sizeof(disk_file_t));
    klee_make_shared(__fs.files[i], sizeof(disk_file_t));

    disk_file_t *dfile = __fs.files[i];

    fname[fname_len-1] = '0' + (i % 10);
    fname[fname_len-2] = '0' + (i / 10);

    __init_disk_file(dfile, file_length, fname, &s, 1);
  }

  // Create the stdin symbolic file
  klee_make_shared(&__stdin_file, sizeof(disk_file_t));
  __init_disk_file(&__stdin_file, MAX_STDINSIZE, "STDIN", &s, 0);
}

static void _init_network(void) {
  // Initialize the INET domain
  klee_make_shared(&__net, sizeof(__net));

  __net.net_addr.s_addr = htonl(DEFAULT_NETWORK_ADDR);
  __net.next_port = htons(DEFAULT_UNUSED_PORT);
  STATIC_LIST_INIT(__net.end_points);

  // Initialize the UNIX domain
  klee_make_shared(&__unix_net, sizeof(__unix_net));
  STATIC_LIST_INIT(__unix_net.end_points);
}

void klee_init_fds(unsigned n_files, unsigned file_length,
                   int sym_stdout_flag) {
  _init_symfiles(n_files, file_length);
  _init_network();

  _init_fdt();
}
