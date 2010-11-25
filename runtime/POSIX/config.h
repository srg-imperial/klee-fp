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
 * config.h - POSIX Model Configuration
 *
 *  Created on: Oct 3, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef POSIX_CONFIG_H_
#define POSIX_CONFIG_H_

////////////////////////////////////////////////////////////////////////////////
// System Limits
////////////////////////////////////////////////////////////////////////////////

#define MAX_THREADS         16
#define MAX_PROCESSES       8

#define MAX_SEMAPHORES      8

#define MAX_EVENTS          4

#define MAX_FDS             64
#define MAX_FILES           16

#define MAX_PATH_LEN        75

#define MAX_PORTS           32
#define MAX_UNIX_EPOINTS    32

#define MAX_PENDING_CONN    4

#define MAX_MMAPS           4

#define MAX_STDINSIZE       16

#define SOCKET_BUFFER_SIZE      4096
#define PIPE_BUFFER_SIZE        4096
#define SENDFILE_BUFFER_SIZE    256

////////////////////////////////////////////////////////////////////////////////
// Enabled Components
////////////////////////////////////////////////////////////////////////////////

#define HAVE_FAULT_INJECTION    1
//#define HAVE_SYMBOLIC_CTYPE     1


#endif /* POSIX_CONFIG_H_ */
