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
 * KleeCommon.h
 *
 *  Created on: Apr 6, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef KLEECOMMON_H_
#define KLEECOMMON_H_

#include "klee/Config/config.h"

#include <string>

#define KLEE_ROOT_VAR	"KLEE_ROOT"
#define KLEE_UCLIBC_ROOT_VAR "KLEE_UCLIBC_ROOT"

std::string getKleePath();
std::string getKleeLibraryPath();
std::string getUclibcPath();


#endif /* KLEECOMMON_H_ */
