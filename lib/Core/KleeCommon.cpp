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
 * KleeCommon.cpp
 *
 *  Created on: Apr 6, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#include "klee/KleeCommon.h"

#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 9)
#include "llvm/System/Path.h"
#else
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/FileSystem.h"
#endif

#include <string>
#include <cstdlib>

using llvm::sys::Path;

#if !(LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 9)
using namespace llvm::sys::fs;
using llvm::SmallString;
#endif

static const std::string &PathToString(Path &path) {
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
  return path.toString();
#else
  return path.str();
#endif
}

static void MakeAbsolute(Path &path) {
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 9)
  path.makeAbsolute();
#else
  SmallString<64> pathStr(llvm::StringRef(path.str()));
  make_absolute(pathStr);
  path = pathStr;
#endif
}

std::string getKleePath() {
	// First look for $KLEE_ROOT, then KLEE_DIR
	char *kleePathName = std::getenv(KLEE_ROOT_VAR);
	Path kleePath;

	if (kleePathName != NULL) {
		// Check whether the path exists
		kleePath = Path(kleePathName);

		if (kleePath.isValid()) {
			// The path exists, so we return it
			MakeAbsolute(kleePath);
			return PathToString(kleePath);
		}
	}

	kleePath = Path(KLEE_DIR);
        MakeAbsolute(kleePath);
	return PathToString(kleePath);
}

std::string getKleeLibraryPath() {
	std::string kleePathName = getKleePath();

	Path libraryPath(kleePathName);
	libraryPath.appendComponent(RUNTIME_CONFIGURATION);
	libraryPath.appendComponent("lib");

	return PathToString(libraryPath);
}

std::string getUclibcPath() {
	char *uclibcPathName = std::getenv(KLEE_UCLIBC_ROOT_VAR);
	Path uclibcPath;

	if (uclibcPathName != NULL) {
		uclibcPath = Path(uclibcPathName);

		if (uclibcPath.isValid()) {
			MakeAbsolute(uclibcPath);
			return PathToString(uclibcPath);
		}
	}

	uclibcPath = Path(KLEE_UCLIBC);
        MakeAbsolute(uclibcPath);

	return PathToString(uclibcPath);
}
