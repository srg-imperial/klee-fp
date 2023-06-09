//===-- InstructionInfoTable.cpp ------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Config/config.h"
#include "klee/Internal/Module/InstructionInfoTable.h"

#include "llvm/Function.h"
#include "llvm/Instructions.h"
#include "llvm/IntrinsicInst.h"
#include "llvm/Linker.h"
#include "llvm/Module.h"
#include "llvm/Assembly/AsmAnnotationWriter.h"
#include "llvm/Support/CFG.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#if !(LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
#include "llvm/Analysis/DebugInfo.h"
#endif
#include "llvm/Analysis/ValueTracking.h"

#include <map>
#include <string>

using namespace llvm;
using namespace klee;

class InstructionToLineAnnotator : public llvm::AssemblyAnnotationWriter {
public:
  void emitInstructionAnnot(const Instruction *i, llvm::raw_ostream &os) {
    os << "%%%" << (uintptr_t) i;
  }
};
        
static void buildInstructionToLineMap(Module *m,
                                      std::map<const Instruction*, unsigned> &out) {  
  InstructionToLineAnnotator a;
  std::string str;
  llvm::raw_string_ostream os(str);
  m->print(os, &a);
  os.flush();
  const char *s;

  unsigned line = 1;
  for (s=str.c_str(); *s; s++) {
    if (*s=='\n') {
      line++;
      if (s[1]=='%' && s[2]=='%' && s[3]=='%') {
        s += 4;
        char *end;
        unsigned long long value = strtoull(s, &end, 10);
        if (end!=s) {
          out.insert(std::make_pair((const Instruction*) value, line));
        }
        s = end;
      }
    }
  }
}

#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
static std::string getDSPIPath(const DbgStopPointInst *dspi) {
  std::string dir, file;
  bool res = GetConstantStringInfo(dspi->getDirectory(), dir);
  assert(res && "GetConstantStringInfo failed");
  res = GetConstantStringInfo(dspi->getFileName(), file);
  assert(res && "GetConstantStringInfo failed");
  if (dir.empty()) {
    return file;
  } else if (*dir.rbegin() == '/') {
    return dir + file;
  } else {
    return dir + "/" + file;
  }
}
#else
static std::string getDSPIPath(DILocation Loc) {
  std::string dir = Loc.getDirectory();
  std::string file = Loc.getFilename();
  if (dir.empty()) {
    return file;
  } else if (*dir.rbegin() == '/') {
    return dir + file;
  } else {
    return dir + "/" + file;
  }
}
#endif

bool klee::getInstructionDebugInfo(const llvm::Instruction *I, 
                                   std::string &File,
                                   unsigned &Line) {
#if (LLVM_VERSION_MAJOR == 2 && LLVM_VERSION_MINOR < 7)
  if (const DbgStopPointInst *dspi = dyn_cast<DbgStopPointInst>(I)) {
    File = getDSPIPath(dspi);
    Line = dspi->getLine();
    return true;
  }
#else
  if (MDNode *N = I->getMetadata("dbg")) {
    DILocation Loc(N);
    File = getDSPIPath(Loc);
    Line = Loc.getLineNumber();
    return true;
  }
#endif

  return false;
}

InstructionInfoTable::InstructionInfoTable(Module *m) 
  : dummyString(""), dummyInfo(0, dummyString, 0, 0) {
  unsigned id = 0;
  std::map<const Instruction*, unsigned> lineTable;
  buildInstructionToLineMap(m, lineTable);

  for (Module::iterator fnIt = m->begin(), fn_ie = m->end(); 
       fnIt != fn_ie; ++fnIt) {
    const std::string *initialFile = &dummyString;
    unsigned initialLine = 0;

    // It may be better to look for the closest stoppoint to the entry
    // following the CFG, but it is not clear that it ever matters in
    // practice.
    for (inst_iterator it = inst_begin(fnIt), ie = inst_end(fnIt);
         it != ie; ++it) {
      std::string file;
      if (getInstructionDebugInfo(&*it, file, initialLine)) {
        initialFile = internString(file); 
        break;
      }
    }
    
    typedef std::map<BasicBlock*, std::pair<const std::string*,unsigned> > 
      sourceinfo_ty;
    sourceinfo_ty sourceInfo;
    for (llvm::Function::iterator bbIt = fnIt->begin(), bbie = fnIt->end(); 
         bbIt != bbie; ++bbIt) {
      std::pair<sourceinfo_ty::iterator, bool>
        res = sourceInfo.insert(std::make_pair(bbIt,
                                               std::make_pair(initialFile,
                                                              initialLine)));
      if (!res.second)
        continue;

      std::vector<BasicBlock*> worklist;
      worklist.push_back(bbIt);

      do {
        BasicBlock *bb = worklist.back();
        worklist.pop_back();

        sourceinfo_ty::iterator si = sourceInfo.find(bb);
        assert(si != sourceInfo.end());
        std::string file;
        const std::string *infile = si->second.first;
        unsigned line = si->second.second;
        
        for (BasicBlock::iterator it = bb->begin(), ie = bb->end();
             it != ie; ++it) {
          Instruction *instr = it;
          unsigned assemblyLine = 0;
          std::map<const Instruction*, unsigned>::const_iterator ltit = 
            lineTable.find(instr);
          if (ltit!=lineTable.end())
            assemblyLine = ltit->second;
          if (getInstructionDebugInfo(instr, file, line))
            infile = internString(file);
          infos.insert(std::make_pair(instr,
                                      InstructionInfo(id++,
                                                      *infile,
                                                      line,
                                                      assemblyLine)));        
        }
        
        for (succ_iterator it = succ_begin(bb), ie = succ_end(bb); 
             it != ie; ++it) {
          if (sourceInfo.insert(std::make_pair(*it,
                                               std::make_pair(infile, line))).second)
            worklist.push_back(*it);
        }
      } while (!worklist.empty());
    }
  }
}

InstructionInfoTable::~InstructionInfoTable() {
  for (std::set<const std::string *, ltstr>::iterator
         it = internedStrings.begin(), ie = internedStrings.end();
       it != ie; ++it)
    delete *it;
}

const std::string *InstructionInfoTable::internString(std::string s) {
  std::set<const std::string *, ltstr>::iterator it = internedStrings.find(&s);
  if (it==internedStrings.end()) {
    std::string *interned = new std::string(s);
    internedStrings.insert(interned);
    return interned;
  } else {
    return *it;
  }
}

unsigned InstructionInfoTable::getMaxID() const {
  return infos.size();
}

const InstructionInfo &
InstructionInfoTable::getInfo(const Instruction *inst) const {
  std::map<const llvm::Instruction*, InstructionInfo>::const_iterator it = 
    infos.find(inst);
  if (it==infos.end()) {
    return dummyInfo;
  } else {
    return it->second;
  }
}

const InstructionInfo &
InstructionInfoTable::getFunctionInfo(const Function *f) const {
  if (f->isDeclaration()) {
    return dummyInfo;
  } else {
    return getInfo(f->begin()->begin());
  }
}
