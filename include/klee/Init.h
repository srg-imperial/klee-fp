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
 * Init.h
 *
 *  Created on: Sep 2, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef INIT_H_
#define INIT_H_

namespace llvm {
class Module;
}

namespace klee {
void externalsAndGlobalsCheck(const llvm::Module *m);
llvm::Module* loadByteCode();
llvm::Module* prepareModule(llvm::Module *module);
void readProgramArguments(int &pArgc, char **&pArgv, char **&pEnvp, char **envp);
}


#endif /* INIT_H_ */
