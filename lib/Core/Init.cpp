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
 * Init.cpp
 *
 *  Created on: Sep 2, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#include "klee/Init.h"
#include "klee/KleeCommon.h"

#include "Common.h"
#include "klee/Config/Version.h"
#include "klee/Internal/Support/ModuleUtil.h"

// FIXME: Ugh, this is gross. But otherwise our config.h conflicts with LLVMs.
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "llvm/Support/CommandLine.h"
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 9)
#include "llvm/System/Path.h"
#else
#include "llvm/Support/Path.h"
#include "llvm/Support/system_error.h"
#endif
#include "llvm/Module.h"
#include "llvm/Type.h"
#include "llvm/InstrTypes.h"
#include "llvm/Instruction.h"
#include "llvm/Instructions.h"
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
#include "llvm/ModuleProvider.h"
#endif
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/LLVMContext.h"
#include "llvm/Constants.h"

#include <iostream>
#include <map>
#include <set>
#include <fstream>

using namespace llvm;
using namespace klee;

enum LibcType {
    NoLibc, UcLibc
};

std::string InputFile;
LibcType Libc;
bool WithPOSIXRuntime, WithOpenCLRuntime;

namespace {
  cl::opt<bool>
  InitEnv("init-env",
      cl::desc("Create custom environment.  Options that can be passed as arguments to the programs are: --sym-argv <max-len>  --sym-argvs <min-argvs> <max-argvs> <max-len> + file model options"));

  cl::opt<bool>
  WarnAllExternals("warn-all-externals",
                   cl::desc("Give initial warning for all externals."));

  cl::opt<std::string>
  Environ("environ", cl::desc("Parse environ from given file (in \"env\" format)"));

  cl::list<std::string>
  InputArgv(cl::ConsumeAfter,
            cl::desc("<program arguments>..."));

  static cl::opt<std::string, true> InputFileOpt(cl::desc("<input bytecode>"), cl::Positional,
          cl::location(InputFile), cl::init("-"));

  static cl::opt<LibcType, true> LibcOpt("libc", cl::desc(
          "Choose libc version (none by default)."), cl::values(
          clEnumValN(NoLibc, "none", "Don't link in a libc"),
            clEnumValN(UcLibc, "uclibc", "Link in uclibc (adapted for klee)"),
            clEnumValEnd) , cl::location(Libc), cl::init(NoLibc));

  static cl::opt<bool, true> WithPOSIXRuntimeOpt("posix-runtime", cl::desc(
          "Link with POSIX runtime"), cl::location(WithPOSIXRuntime), cl::init(false));

  static cl::opt<bool, true> WithOpenCLRuntimeOpt("opencl-runtime", cl::desc(
          "Link with OpenCL runtime"), cl::location(WithOpenCLRuntime), cl::init(false));

}

namespace klee {

static std::string strip(std::string &in) {
    unsigned len = in.size();
    unsigned lead = 0, trail = len;
    while (lead < len && isspace(in[lead]))
        ++lead;
    while (trail > lead && isspace(in[trail - 1]))
        --trail;
    return in.substr(lead, trail - lead);
}

static int initEnv(Module *mainModule) {

  /*
    nArgcP = alloc oldArgc->getType()
    nArgvV = alloc oldArgv->getType()
    store oldArgc nArgcP
    store oldArgv nArgvP
    klee_init_environment(nArgcP, nArgvP)
    nArgc = load nArgcP
    nArgv = load nArgvP
    oldArgc->replaceAllUsesWith(nArgc)
    oldArgv->replaceAllUsesWith(nArgv)
  */

  Function *mainFn = mainModule->getFunction("main");

  if (mainFn->arg_size() < 2) {
    std::cerr << "Cannot handle ""-init-env"" when main() has less than two arguments.\n";
    return -1;
  }

  Instruction* firstInst = mainFn->begin()->begin();

  Value* oldArgc = mainFn->arg_begin();
  Value* oldArgv = ++mainFn->arg_begin();

  AllocaInst* argcPtr =
    new AllocaInst(oldArgc->getType(), "argcPtr", firstInst);
  AllocaInst* argvPtr =
    new AllocaInst(oldArgv->getType(), "argvPtr", firstInst);

  /* Insert void klee_init_env(int* argc, char*** argv) */
  std::vector<const Type*> params;
  params.push_back(Type::getInt32Ty(getGlobalContext()));
  params.push_back(Type::getInt32Ty(getGlobalContext()));
  Function* initEnvFn =
    cast<Function>(mainModule->getOrInsertFunction("klee_init_env",
                                                   Type::getVoidTy(getGlobalContext()),
                                                   argcPtr->getType(),
                                                   argvPtr->getType(),
                                                   NULL));
  assert(initEnvFn);
  std::vector<Value*> args;
  args.push_back(argcPtr);
  args.push_back(argvPtr);
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 0)
  Instruction* initEnvCall = CallInst::Create(initEnvFn, args,
                          "", firstInst);
#else
  Instruction* initEnvCall = CallInst::Create(initEnvFn, args.begin(), args.end(),
                          "", firstInst);
#endif
  Value *argc = new LoadInst(argcPtr, "newArgc", firstInst);
  Value *argv = new LoadInst(argvPtr, "newArgv", firstInst);

  oldArgc->replaceAllUsesWith(argc);
  oldArgv->replaceAllUsesWith(argv);

  new StoreInst(oldArgc, argcPtr, initEnvCall);
  new StoreInst(oldArgv, argvPtr, initEnvCall);

  return 0;
}


// This is a terrible hack until we get some real modelling of the
// system. All we do is check the undefined symbols and m and warn about
// any "unrecognized" externals and about any obviously unsafe ones.

// Symbols we explicitly support
static const char *modelledExternals[] = {
  "_ZTVN10__cxxabiv117__class_type_infoE",
  "_ZTVN10__cxxabiv120__si_class_type_infoE",
  "_ZTVN10__cxxabiv121__vmi_class_type_infoE",

  // special functions
  "_assert",
  "__assert_fail",
  "__assert_rtn",
  "calloc",
  "_exit",
  "exit",
  "free",
  "abort",
  "klee_abort",
  "klee_assume",
  "klee_check_memory_access",
  "klee_define_fixed_object",
  "klee_get_errno",
  "klee_get_value",
  "klee_get_obj_size",
  "klee_is_symbolic",
  "klee_make_symbolic",
  "klee_mark_global",
  "klee_merge",
  "klee_prefer_cex",
  "klee_print_expr",
  "klee_print_c_expr",
  "klee_print_range",
  "klee_report_error",
  "klee_set_forking",
  "klee_silent_exit",
  "klee_warning",
  "klee_warning_once",
  "klee_alias_function",
  "klee_stack_trace",
  "klee_dump_constraints",
  "klee_watch",
  "klee_ocl_compile",
  "klee_lookup_module_global",
  "klee_ocl_get_arg_type",
  "klee_ocl_get_arg_count",
  "klee_make_shared",
  "klee_bind_shared",
  "llvm.dbg.stoppoint",
  "llvm.va_start",
  "llvm.va_end",
  "malloc",
  "realloc",
  "valloc",
  "_ZdaPv",
  "_ZdlPv",
  "_Znaj",
  "_Znwj",
  "_Znam",
  "_Znwm",
};
// Symbols we aren't going to warn about
static const char *dontCareExternals[] = {
#if 0
  // stdio
  "fprintf",
  "fflush",
  "fopen",
  "fclose",
  "fputs_unlocked",
  "putchar_unlocked",
  "vfprintf",
  "fwrite",
  "puts",
  "printf",
  "stdin",
  "stdout",
  "stderr",
  "_stdio_term",
  "__errno_location",
  "fstat",
#endif

  // static information, pretty ok to return
  "getegid",
  "geteuid",
  "getgid",
  "getuid",
  "getpid",
  "gethostname",
  "getpgrp",
  "getppid",
  "getpagesize",
  "getpriority",
  "getgroups",
  "getdtablesize",
  "getrlimit",
  "getrlimit64",
  "getcwd",
  "getwd",
  "gettimeofday",
  "uname",

  // fp stuff we just don't worry about yet
  "frexp",
  "ldexp",
  "__isnan",
  "__signbit",
};

// Extra symbols we aren't going to warn about with uclibc
static const char *dontCareUclibc[] = {
  "__dso_handle",

  // Don't warn about these since we explicitly commented them out of
  // uclibc.
  "printf",
  "vprintf"
};
// Symbols we consider unsafe
static const char *unsafeExternals[] = {
  "fork", // oh lord
  "exec", // heaven help us
  "error", // calls _exit
  "raise", // yeah
  "kill", // mmmhmmm
};
#define NELEMS(array) (sizeof(array)/sizeof(array[0]))


void externalsAndGlobalsCheck(const Module *m) {
  std::map<std::string, bool> externals;
  std::set<std::string> modelled(modelledExternals,
                                 modelledExternals+NELEMS(modelledExternals));
  std::set<std::string> dontCare(dontCareExternals,
                                 dontCareExternals+NELEMS(dontCareExternals));
  std::set<std::string> unsafe(unsafeExternals,
                               unsafeExternals+NELEMS(unsafeExternals));

  switch (Libc) {
  case UcLibc:
    dontCare.insert(dontCareUclibc,
                    dontCareUclibc+NELEMS(dontCareUclibc));
    break;
  case NoLibc: /* silence compiler warning */
    break;
  }


  if (WithPOSIXRuntime)
    dontCare.insert("syscall");


  for (Module::const_iterator fnIt = m->begin(), fn_ie = m->end();
       fnIt != fn_ie; ++fnIt) {
    if (fnIt->isDeclaration() && !fnIt->use_empty())
      externals.insert(std::make_pair(fnIt->getName(), false));
    for (Function::const_iterator bbIt = fnIt->begin(), bb_ie = fnIt->end();
     bbIt != bb_ie; ++bbIt) {
      for (BasicBlock::const_iterator it = bbIt->begin(), ie = bbIt->end();
           it != ie; ++it) {
        if (const CallInst *ci = dyn_cast<CallInst>(it)) {
          if (isa<InlineAsm>(ci->getCalledValue())) {
            klee_warning_once(&*fnIt,
                              "function \"%s\" has inline asm",
                              fnIt->getName().data());
          }
        }
      }
    }
  }


  for (Module::const_global_iterator
         it = m->global_begin(), ie = m->global_end();
       it != ie; ++it)
    if (it->isDeclaration() && !it->use_empty())
      externals.insert(std::make_pair(it->getName(), true));
  // and remove aliases (they define the symbol after global
  // initialization)
  for (Module::const_alias_iterator
         it = m->alias_begin(), ie = m->alias_end();
       it != ie; ++it) {
    std::map<std::string, bool>::iterator it2 =
      externals.find(it->getName());
    if (it2!=externals.end())
      externals.erase(it2);
  }

  std::map<std::string, bool> foundUnsafe;
  for (std::map<std::string, bool>::iterator
         it = externals.begin(), ie = externals.end();
       it != ie; ++it) {
    const std::string &ext = it->first;
    if (!modelled.count(ext) && (WarnAllExternals ||
                                 !dontCare.count(ext))) {
      if (unsafe.count(ext)) {
        foundUnsafe.insert(*it);
      } else {
        klee_warning("undefined reference to %s: %s",
                     it->second ? "variable" : "function",
                     ext.c_str());
      }
    }
  }

  for (std::map<std::string, bool>::iterator
         it = foundUnsafe.begin(), ie = foundUnsafe.end();
       it != ie; ++it) {
    const std::string &ext = it->first;
    klee_warning("undefined reference to %s: %s (UNSAFE)!",
                 it->second ? "variable" : "function",
                 ext.c_str());
  }
}



#ifndef KLEE_UCLIBC
static llvm::Module *linkWithUclibc(llvm::Module *mainModule) {
  fprintf(stderr, "error: invalid libc, no uclibc support!\n");
  exit(1);
  return 0;
}
#else

static llvm::Module *linkWithPOSIX(llvm::Module *mainModule) {
  mainModule->getOrInsertFunction("__force_model_linkage",
      Type::getVoidTy(getGlobalContext()), NULL);

  //mainModule->getOrInsertFunction("_exit",
  //    Type::getVoidTy(getGlobalContext()),
  //    Type::getInt32Ty(getGlobalContext()), NULL);

  llvm::sys::Path Path(getKleeLibraryPath());
  Path.appendComponent("libkleeRuntimePOSIX.bca");
  klee_message("NOTE: Using model: %s", Path.c_str());
  mainModule = klee::linkWithLibrary(mainModule, Path.c_str());
  assert(mainModule && "unable to link with simple model");

  for (Module::iterator it = mainModule->begin(); it != mainModule->end(); it++) {
    Function *f = it;
    StringRef fName = f->getName();

    if (!fName.startswith("__klee_model_"))
      continue;

    StringRef newName = fName.substr(strlen("__klee_model_"), fName.size());

    GlobalValue *modelF = mainModule->getNamedValue(newName);

    if (modelF != NULL) {
      //CLOUD9_DEBUG("Patching " << fName.str());
      //modelF->getType()->dump();
      //f->getType()->dump();
      modelF->replaceAllUsesWith(ConstantExpr::getBitCast(f, modelF->getType()));
    }
  }

  for (Module::iterator it = mainModule->begin(); it != mainModule->end();) {
    Function *f = it;
    StringRef fName = f->getName();

    if (!fName.startswith("__klee_original_")) {
      it++;
      continue;
    }

    StringRef newName = fName.substr(strlen("__klee_original_"));

    //CLOUD9_DEBUG("Patching " << fName.str());

    GlobalValue *originalF = mainModule->getNamedValue(newName);

    if (originalF) {
      f->replaceAllUsesWith(ConstantExpr::getBitCast(originalF, f->getType()));
      it++;
      f->eraseFromParent();
    } else {
      // We switch to strings in order to avoid memory errors due to StringRef
      // destruction inside setName().
      std::string newNameStr = newName.str();
      f->setName(newNameStr);
      assert(f->getName().str() == newNameStr);
      it++;
    }

  }

  return mainModule;
}

static void __fix_linkage(llvm::Module *mainModule, std::string libcSymName, std::string libcAliasName) {
  //CLOUD9_DEBUG("Fixing linkage for " << libcSymName);
  Function *libcSym = mainModule->getFunction(libcSymName);
  if (libcSym == NULL)
    return;

  Value *libcAlias = mainModule->getNamedValue(libcAliasName);
  if (libcAlias != NULL) {
    //CLOUD9_DEBUG("Found the alias");
    libcSym->replaceAllUsesWith(libcAlias);
    if (dyn_cast_or_null<GlobalAlias>(libcAlias)) {
      //CLOUD9_DEBUG("Fixing back the alias");
      dyn_cast_or_null<GlobalAlias>(libcAlias)->setAliasee(libcSym);
    }
  } else {
    libcSym->setName(libcAliasName);
    assert(libcSym->getName() == libcAliasName);
  }
}

#define FIX_LINKAGE(module, syscall) \
  __fix_linkage(module, "__libc_" #syscall, #syscall)

static llvm::Module *linkWithUclibc(llvm::Module *mainModule) {
  Function *f;
  // force import of __uClibc_main
  mainModule->getOrInsertFunction("__uClibc_main",
                                  FunctionType::get(Type::getVoidTy(getGlobalContext()),
                                                    std::vector<LLVM_TYPE_Q Type*>(),
                                                    true));

  // force various imports
  if (WithPOSIXRuntime) {
    LLVM_TYPE_Q llvm::Type *i8Ty = Type::getInt8Ty(getGlobalContext());
    mainModule->getOrInsertFunction("realpath",
                                    PointerType::getUnqual(i8Ty),
                                    PointerType::getUnqual(i8Ty),
                                    PointerType::getUnqual(i8Ty),
                                    NULL);
    mainModule->getOrInsertFunction("getutent",
                                    PointerType::getUnqual(i8Ty),
                                    NULL);
    mainModule->getOrInsertFunction("__fgetc_unlocked",
                                    Type::getInt32Ty(getGlobalContext()),
                                    PointerType::getUnqual(i8Ty),
                                    NULL);
    mainModule->getOrInsertFunction("__fputc_unlocked",
                                    Type::getInt32Ty(getGlobalContext()),
                                    Type::getInt32Ty(getGlobalContext()),
                                    PointerType::getUnqual(i8Ty),
                                    NULL);
  }

  f = mainModule->getFunction("__ctype_get_mb_cur_max");
  if (f) f->setName("_stdlib_mb_cur_max");


  // Strip of asm prefixes for 64 bit versions because they are not
  // present in uclibc and we want to make sure stuff will get
  // linked. In the off chance that both prefixed and unprefixed
  // versions are present in the module, make sure we don't create a
  // naming conflict.
  for (Module::iterator fi = mainModule->begin(), fe = mainModule->end();
       fi != fe;) {
    Function *f = fi;
    ++fi;
    const std::string &name = f->getName();

    if(name.compare("__strdup") == 0 ) {
      if (Function *StrDup = mainModule->getFunction("strdup")) {
    f->replaceAllUsesWith(StrDup);
    f->eraseFromParent();
      } else {
    f->setName("strdup");
      }
      continue;
    }

    if (name[0]=='\01') {
      unsigned size = name.size();
      if (name[size-2]=='6' && name[size-1]=='4') {
        std::string unprefixed = name.substr(1);

        // See if the unprefixed version exists.
        if (Function *f2 = mainModule->getFunction(unprefixed)) {
          f->replaceAllUsesWith(f2);
          f->eraseFromParent();
        } else {
          f->setName(unprefixed);
        }
      }
    }
  }

  mainModule = klee::linkWithLibrary(mainModule, getUclibcPath().append("/lib/libc.a"));
  assert(mainModule && "unable to link with uclibc");

  // more sighs, this is horrible but just a temp hack
  //    f = mainModule->getFunction("__fputc_unlocked");
  //    if (f) f->setName("fputc_unlocked");
  //    f = mainModule->getFunction("__fgetc_unlocked");
  //    if (f) f->setName("fgetc_unlocked");

  FIX_LINKAGE(mainModule, open);
  FIX_LINKAGE(mainModule, fcntl);
  FIX_LINKAGE(mainModule, lseek);

  // XXX we need to rearchitect so this can also be used with
  // programs externally linked with uclibc.

  // We now need to swap things so that __uClibc_main is the entry
  // point, in such a way that the arguments are passed to
  // __uClibc_main correctly. We do this by renaming the user main
  // and generating a stub function to call __uClibc_main. There is
  // also an implicit cooperation in that runFunctionAsMain sets up
  // the environment arguments to what uclibc expects (following
  // argv), since it does not explicitly take an envp argument.
  Function *userMainFn = mainModule->getFunction("main");
  assert(userMainFn && "unable to get user main");
  Function *uclibcMainFn = mainModule->getFunction("__uClibc_main");
  assert(uclibcMainFn && "unable to get uclibc main");
  userMainFn->setName("__user_main");

  const FunctionType *ft = uclibcMainFn->getFunctionType();
  assert(ft->getNumParams() == 7);

  std::vector<LLVM_TYPE_Q Type*> fArgs;
  fArgs.push_back(ft->getParamType(1)); // argc
  fArgs.push_back(ft->getParamType(2)); // argv
  Function *stub = Function::Create(FunctionType::get(Type::getInt32Ty(getGlobalContext()), fArgs, false),
                      GlobalVariable::ExternalLinkage,
                      "main",
                      mainModule);
  BasicBlock *bb = BasicBlock::Create(getGlobalContext(), "entry", stub);

  std::vector<llvm::Value*> args;
  args.push_back(llvm::ConstantExpr::getBitCast(userMainFn,
                                                ft->getParamType(0)));
  args.push_back(stub->arg_begin()); // argc
  args.push_back(++stub->arg_begin()); // argv
  args.push_back(Constant::getNullValue(ft->getParamType(3))); // app_init
  args.push_back(Constant::getNullValue(ft->getParamType(4))); // app_fini
  args.push_back(Constant::getNullValue(ft->getParamType(5))); // rtld_fini
  args.push_back(Constant::getNullValue(ft->getParamType(6))); // stack_end
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 0)
  CallInst::Create(uclibcMainFn, args, "", bb);
#else
  CallInst::Create(uclibcMainFn, args.begin(), args.end(), "", bb);
#endif

  new UnreachableInst(getGlobalContext(), bb);

  return mainModule;
}

static llvm::Module *linkWithOpenCL(llvm::Module *mainModule) {
  llvm::sys::Path Path(getKleeLibraryPath());
  Path.appendComponent("libkleeRuntimeCLHost.bca");
  klee_message("NOTE: Using model: %s", Path.c_str());
  mainModule = klee::linkWithLibrary(mainModule, Path.c_str());
  assert(mainModule && "unable to link with simple model");

  return mainModule;
}

Module* loadByteCode() {
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
  std::string ErrorMsg;
  ModuleProvider *MP = 0;
  if (MemoryBuffer *Buffer = MemoryBuffer::getFileOrSTDIN(InputFile, &ErrorMsg)) {
    MP = getBitcodeModuleProvider(Buffer, getGlobalContext(), &ErrorMsg);
    if (!MP) delete Buffer;
  }

  if (!MP)
    klee_error("error loading program '%s': %s", InputFile.c_str(), ErrorMsg.c_str());

  Module *mainModule = MP->materializeModule();
  MP->releaseModule();
  delete MP;
#else
  std::string ErrorMsg;
  Module *mainModule = 0;
  OwningPtr<MemoryBuffer> Buffer;
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 9)
  Buffer.reset(MemoryBuffer::getFileOrSTDIN(InputFile, &ErrorMsg));
#else
  error_code EC = MemoryBuffer::getFileOrSTDIN(InputFile, Buffer);
  if (!Buffer)
    ErrorMsg = EC.message();
#endif
  if (Buffer) {
    mainModule = getLazyBitcodeModule(Buffer.get(), getGlobalContext(),
                                      &ErrorMsg);
  }
  if (mainModule) {
    if (mainModule->MaterializeAllPermanently(&ErrorMsg)) {
      delete mainModule;
      mainModule = 0;
    }
  }
  if (!mainModule)
    klee_error("error loading program '%s': %s", InputFile.c_str(),
               ErrorMsg.c_str());
  Buffer.take();
#endif

  return mainModule;
}

Module* prepareModule(Module *module) {
  if (WithPOSIXRuntime)
    InitEnv = true;

  if (InitEnv) {
    int r = initEnv(module);
    if (r != 0)
      return NULL;
  }

  llvm::sys::Path LibraryDir(getKleeLibraryPath());

  switch (Libc) {
  case NoLibc: /* silence compiler warning */
    break;

  case UcLibc:
    module = linkWithUclibc(module);
    break;
  }

  if (WithPOSIXRuntime) {
    module = linkWithPOSIX(module);
    module = linkWithUclibc(module);
  }

  if (WithOpenCLRuntime)
    module = linkWithOpenCL(module);

  return module;
}

void readProgramArguments(int &pArgc, char **&pArgv, char **&pEnvp, char **envp) {
    if (Environ != "") {
        std::vector<std::string> items;
        std::ifstream f(Environ.c_str());
        if (!f.good())
            klee_error("unable to open --environ file");

        while (!f.eof()) {
            std::string line;
            std::getline(f, line);
            line = strip(line);
            if (!line.empty())
                items.push_back(line);
        }
        f.close();
        pEnvp = new char *[items.size() + 1];
        unsigned i = 0;
        for (; i != items.size(); ++i)
            pEnvp[i] = strdup(items[i].c_str());
        pEnvp[i] = 0;
    } else {
        pEnvp = envp;
    }

    pArgc = InputArgv.size() + 1;
    pArgv = new char *[pArgc];
    for (unsigned i = 0; i < InputArgv.size() + 1; i++) {
        std::string &arg = (i == 0 ? InputFile : InputArgv[i - 1]);
        unsigned size = arg.size() + 1;
        char *pArg = new char[size];

        std::copy(arg.begin(), arg.end(), pArg);
        pArg[size - 1] = 0;

        pArgv[i] = pArg;
    }
}

}

#endif
