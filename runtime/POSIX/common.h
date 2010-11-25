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
 * common.h
 *
 *  Created on: Aug 6, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef COMMON_H_
#define COMMON_H_

#include "config.h"

#include <sys/types.h>

#ifdef __USE_MISC
#undef __USE_MISC
#define __REDEF_MISC
#endif

#ifdef __USE_XOPEN2K8
#undef __USE_XOPEN2K8
#define __REDEF_XOPEN2K8
#endif

#include <sys/stat.h>

#ifdef __REDEF_MISC
#define __USE_MISC 1
#undef __REDEF_MISC
#endif

#ifdef __REDEF_XOPEN2K8
#define __USE_XOPEN2K8
#undef __REDEF_XOPEN2K8
#endif

#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// IOCtl Codes
////////////////////////////////////////////////////////////////////////////////

#define KLEE_SIO_SYMREADS   0xfff00     // Enable symbolic reads for a socket
#define KLEE_SIO_READCAP    0xfff01     // Set a maximum cap for reading from the stream

////////////////////////////////////////////////////////////////////////////////
// Klee Event Codes
////////////////////////////////////////////////////////////////////////////////

#define __KLEE_EVENT_PKT_FRAGMENT   1337


////////////////////////////////////////////////////////////////////////////////
// Model Infrastructure
////////////////////////////////////////////////////////////////////////////////

// A model needs to be declared only when it's supposed to interface
// an existing C library call.

#define __DECLARE_MODEL(type, name, ...) \
  type __klee_model_ ## name(__VA_ARGS__); \
  type __klee_original_ ## name(__VA_ARGS__);

#ifdef __FORCE_USE_MODELS
#define DECLARE_MODEL(type, name, ...) \
  __DECLARE_MODEL(type, name, ##__VA_ARGS__) \
  __attribute__((used)) static const void* __usage_model_ ## name = (void*)&__klee_model_ ## name;
#else
#define DECLARE_MODEL(type, name, ...) \
  __DECLARE_MODEL(type, name, ##__VA_ARGS__)
#endif

#define FORCE_IMPORT(name) \
  __attribute__((used)) static const void* __usage_ ## name = (void*) &name;

#define CALL_UNDERLYING(name, ...) \
    __klee_original_ ## name(__VA_ARGS__);

#define CALL_MODEL(name, ...) \
    __klee_model_ ## name(__VA_ARGS__);

#define DEFINE_MODEL(type, name, ...) \
    type __klee_model_ ## name(__VA_ARGS__)


#ifdef HAVE_FAULT_INJECTION

#define INJECT_FAULT(name, ...) \
    __inject_fault(#name, ##__VA_ARGS__)

#else

#define INJECT_FAULT(name, ...)     0

#endif

int klee_get_errno(void);

void *__concretize_ptr(const void *p);
size_t __concretize_size(size_t s);
const char *__concretize_string(const char *s);

unsigned __fork_values(unsigned min, unsigned max, int reason);

int __inject_fault(const char *fname, int errno, ...);

////////////////////////////////////////////////////////////////////////////////
// Basic Arrays
////////////////////////////////////////////////////////////////////////////////

// XXX Remove this, since it's unused?

#define ARRAY_INIT(arr) \
  do { memset(&arr, 0, sizeof(arr)); } while (0)

#define ARRAY_ALLOC(arr, item) \
  do { \
    item = sizeof(arr)/sizeof(arr[0]); \
    unsigned int __i; \
    for (__i = 0; __i < sizeof(arr)/sizeof(arr[0]); __i++) { \
      if (!arr[__i]) { \
        item = __i; break; \
      } \
    } \
  } while (0)

#define ARRAY_CLEAR(arr, item) \
  do { arr[item] = 0; } while (0)

#define ARRAY_CHECK(arr, item) \
  ((item < sizeof(arr)/sizeof(arr[0])) && arr[item] != 0)


////////////////////////////////////////////////////////////////////////////////
// Static Lists
////////////////////////////////////////////////////////////////////////////////

#define STATIC_LIST_INIT(list)  \
  do { memset(&list, 0, sizeof(list)); } while (0)

#define STATIC_LIST_ALLOC(list, item) \
  do { \
    item = sizeof(list)/sizeof(list[0]); \
    unsigned int __i; \
    for (__i = 0; __i < sizeof(list)/sizeof(list[0]); __i++) { \
      if (!list[__i].allocated) { \
        list[__i].allocated = 1; \
        item = __i;  break; \
      } \
    } \
  } while (0)

#define STATIC_LIST_CLEAR(list, item) \
  do { memset(&list[item], 0, sizeof(list[item])); } while (0)

#define STATIC_LIST_CHECK(list, item) \
  (((item) < sizeof(list)/sizeof(list[0])) && (list[item].allocated))

#endif /* COMMON_H_ */
