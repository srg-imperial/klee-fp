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
 * chroot_perf.c - Determines the performance of the chroot() system call,
 * in terms of # of operations per second.
 *
 *  Created on: Nov 11, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>

static void go_jail(int jailfd) {
  int res;

  uid_t uid = getuid();

  // First, go in the jail
  res = seteuid(0);
  assert(res == 0);

  res = fchdir(jailfd);
  assert(res == 0);

  res = chroot(".");
  assert(res == 0);

  res = seteuid(uid);
  assert(res == 0);
}

void go_back(int rootfd, uid_t uid) {
  int res;
}

void get_totaltime(struct timeval *timeval) {
  struct rusage r;
  int res = getrusage(RUSAGE_SELF, &r);
  assert(res == 0);

  timeradd(&r.ru_stime, &r.ru_utime, timeval);
}

int main(int argc, char **argv) {
  int oldroot = open("/", O_RDONLY | O_DIRECTORY);
  assert(oldroot != -1);

  int newroot = open(".", O_RDONLY | O_DIRECTORY);
  assert(newroot != -1);

  int res;

  unsigned long counter = 0;

  struct timeval old_time;
  get_totaltime(&old_time);

  for (;;) {
    // First, go in the jail
    go_jail(newroot);

    // Make sure we cannot open a system file
    res = open("/etc/passwd", O_RDONLY);
    assert(res == -1);

    // Now go back
    go_jail(oldroot);

    // Check that now we can open the system file
    res = open("/etc/passwd", O_RDONLY);
    assert(res >= 0);
    close(res);

    counter++;

    if (counter % 100 == 0) {
      struct timeval new_time;
      get_totaltime(&new_time);

      struct timeval time_diff;
      timersub(&new_time, &old_time, &time_diff);

      if (time_diff.tv_sec >= 1) {
        fprintf(stderr, "%lu ops/second\n", counter);
        counter = 0;
        old_time = new_time;
      }
    }
  }

  return 0;
}
