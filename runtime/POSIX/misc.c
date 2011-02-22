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
 * misc.c
 *
 *  Created on: Sep 11, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#include "misc.h"
#include "models.h"

#include <sched.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>

#include <klee/klee.h>

////////////////////////////////////////////////////////////////////////////////
// Sleeping Operations
////////////////////////////////////////////////////////////////////////////////

int usleep(useconds_t usec) {
  klee_warning("yielding instead of usleep()-ing");

  uint64_t tstart = klee_get_time();
  klee_thread_preempt(1);
  uint64_t tend = klee_get_time();

  if (tend - tstart < (uint64_t)usec)
    klee_set_time(tstart + usec);

  return 0;
}

unsigned int sleep(unsigned int seconds) {
  klee_warning("yielding instead of sleep()-ing");

  uint64_t tstart = klee_get_time();
  klee_thread_preempt(1);
  uint64_t tend = klee_get_time();

  if (tend - tstart < ((uint64_t)seconds)*1000000) {
    klee_set_time(tstart + ((uint64_t)seconds) * 1000000);
  }

  return 0;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
  if (tv) {
    uint64_t ktime = klee_get_time();
    tv->tv_sec = ktime / 1000000;
    tv->tv_usec = ktime % 1000000;
  }

  if (tz) {
    tz->tz_dsttime = 0;
    tz->tz_minuteswest = 0;
  }

  return 0;
}

int settimeofday(const struct timeval *tv, const struct timezone *tz) {
  if (tv) {
    uint64_t ktime = tv->tv_sec * 1000000 + tv->tv_usec;
    klee_set_time(ktime);
  }

  if (tz) {
    klee_warning("ignoring timezone set request");
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// mmap Operations
////////////////////////////////////////////////////////////////////////////////

mmap_block_t __mmaps[MAX_MMAPS];

void klee_init_mmap(void) {
  STATIC_LIST_INIT(__mmaps);
}

static int _mmap_prepopulate(void *addr, size_t length, int fd, off_t offset) {
  off_t origpos = CALL_MODEL(lseek, fd, SEEK_CUR, 0);

  if (origpos == -1)
    goto invalid;

  off_t newpos = CALL_MODEL(lseek, fd, SEEK_SET, offset);
  if (newpos == -1)
    goto invalid;

  size_t remaining = length;
  char *dest = addr;

  while (remaining > 0) {
    ssize_t res = read(fd, dest, remaining);

    if (res > 0) {
      dest += res;
      remaining -= res;
    } else if (res == 0) {
      // Could not read everything, it's OK
      break;
    } else {
      goto invalid;
    }
  }

  CALL_MODEL(lseek, fd, SEEK_SET, origpos);

  return 0;

invalid:
  if (origpos >= 0)
    CALL_MODEL(lseek, fd, SEEK_SET, origpos);

  errno = EINVAL;
  return -1;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
  if ((prot & (PROT_READ | PROT_WRITE)) != (PROT_READ | PROT_WRITE)) {
    klee_warning("unsupported read- or write-only mapping, going on anyway");
  }

  if (INJECT_FAULT(mmap, ENOMEM)) {
    return MAP_FAILED;
  }

  // We try allocating the memory
  void *result = malloc(length);

  if (!result) {
    errno = ENOMEM;
    return MAP_FAILED;
  }

  memset(result, 0, length);

  // We pre-populate the mapping
  if (flags & MAP_ANONYMOUS) {
    if (fd >= 0) {
      free(result);

      errno = EINVAL;
      return MAP_FAILED;
    }
  } else {
    int res = _mmap_prepopulate(result, length, fd, offset);

    if (res == -1) {
      free(result);
      return MAP_FAILED;
    }
  }

  // Now we create the mapping
  unsigned int idx;
  STATIC_LIST_ALLOC(__mmaps, idx);

  if (idx == MAX_MMAPS) {
    free(result);

    errno = ENOMEM;
    return MAP_FAILED;
  }

  __mmaps[idx].addr = result;
  __mmaps[idx].length = length;
  __mmaps[idx].prot = prot;
  __mmaps[idx].flags = flags;

  if (flags & MAP_SHARED) {
    klee_make_shared(result, length);
  }

  return result;
}

void *mmap2(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset) {
  return mmap(addr, length, prot, flags, fd, pgoffset * getpagesize());
}

int munmap(void *addr, size_t length) {
  unsigned idx;
  for (idx = 0; idx < MAX_MMAPS; idx++) {
    if (!STATIC_LIST_CHECK(__mmaps, idx))
      continue;

    if (__mmaps[idx].addr == addr)
      break;
  }

  if (idx == MAX_MMAPS) {
    klee_warning("inexistent mapping or unsupported fragment unmapping");
    errno = EINVAL;
    return -1;
  }

  assert(__mmaps[idx].addr);

  if (__mmaps[idx].length != length) {
    klee_warning("unsupported fragment unmapping");
    errno = EINVAL;
    return -1;
  }

  free(__mmaps[idx].addr);

  STATIC_LIST_CLEAR(__mmaps, idx);

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// memalign and friends
////////////////////////////////////////////////////////////////////////////////

// Currently we don't respect the requested alignment.
void *memalign(size_t boundary, size_t size) {
  return malloc(size);
}

////////////////////////////////////////////////////////////////////////////////
// Misc. API
////////////////////////////////////////////////////////////////////////////////

int sched_yield(void) {
  klee_thread_preempt(1);
  return 0;
}

int getrusage(int who, struct rusage *usage) {
  if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN && who != RUSAGE_THREAD) {
    errno = EINVAL;
    return -1;
  }

  memset(usage, 0, sizeof(*usage)); // XXX Refine this as further needed

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_SYMBOLIC_CTYPE
static const int32_t *tolower_locale = NULL;

DEFINE_MODEL(const int32_t **, __ctype_tolower_loc, void) {
  if (tolower_locale != NULL)
    return &tolower_locale;

  int32_t *cached = (int32_t*)malloc(384*sizeof(int32_t));
  klee_make_shared(cached, 384*sizeof(int32_t));

  const int32_t **locale = CALL_UNDERLYING(__ctype_tolower_loc);

  memcpy(cached, &((*locale)[-128]), 384*sizeof(int32_t));

  tolower_locale = &cached[128];

  return &tolower_locale;
}

static const unsigned short *b_locale = NULL;

DEFINE_MODEL(const unsigned short **, __ctype_b_loc, void) {
  if (b_locale != NULL)
    return &b_locale;

  unsigned short *cached = (unsigned short*)malloc(384*sizeof(unsigned short));
  klee_make_shared(cached, 384*sizeof(unsigned short));

  const unsigned short **locale = CALL_UNDERLYING(__ctype_b_loc);

  memcpy(cached, &((*locale)[-128]), 384*sizeof(unsigned short));

  b_locale = &cached[128];

  return &b_locale;
}
#endif
