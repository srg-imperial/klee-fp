/*
 * stubs.h
 *
 *  Created on: Sep 30, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 */

#ifndef STUBS_H_
#define STUBS_H_

#include <stdio.h>
#include <stdint.h>

#ifdef  __cplusplus
extern "C" {
#endif

void klee_make_symbolic(void *addr, size_t nbytes, const char *name) __attribute__((weak));
void klee_event(unsigned int type, long int value) __attribute__((weak));
void klee_assume(uintptr_t condition) __attribute__((weak));

#define KLEE_SIO_SYMREADS   0xfff00     // Enable symbolic reads for a socket
#define KLEE_SIO_READCAP    0xfff01     // Set a maximum cap for reading from the stream

#ifdef  __cplusplus
}
#endif


#endif /* STUBS_H_ */
