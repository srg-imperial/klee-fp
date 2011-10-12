//===-- Memory.h ------------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_MEMORY_H
#define KLEE_MEMORY_H

#include "Context.h"
#include "klee/Expr.h"
#include "klee/Threading.h"

#include "llvm/ADT/StringExtras.h"

#include <vector>
#include <string>

namespace llvm {
  class Value;
}

namespace klee {

class BitArray;
class ExecutionState;
class MemoryManager;
class Solver;
class TimingSolver;

class MemoryObject {
  friend class STPBuilder;

private:
  static int counter;

public:
  unsigned id;
  uint64_t address;

  /// size in bytes
  unsigned size;
  mutable std::string name;

  bool isLocal;
  mutable bool isGlobal;
  bool isFixed;

  /// true if created by us.
  bool fake_object;
  bool isUserSpecified;

  /// "Location" for which this memory object was allocated. This
  /// should be either the allocating instruction or the global object
  /// it was allocated for (or whatever else makes sense).
  const llvm::Value *allocSite;
  
  /// A list of boolean expressions the user has requested be true of
  /// a counterexample. Mutable since we play a little fast and loose
  /// with allowing it to be added to during execution (although
  /// should sensibly be only at creation time).
  mutable std::vector< ref<Expr> > cexPreferences;

  // DO NOT IMPLEMENT
  MemoryObject(const MemoryObject &b);
  MemoryObject &operator=(const MemoryObject &b);

public:

  // XXX this is just a temp hack, should be removed
  explicit
  MemoryObject(uint64_t _address) 
    : id(counter++),
      address(_address),
      size(0),
      isFixed(true),
      allocSite(0) {
  }


  MemoryObject(uint64_t _address, unsigned _size, 
               bool _isLocal, bool _isGlobal, bool _isFixed,
               const llvm::Value *_allocSite) 
    : id(counter++),
      address(_address),
      size(_size),
      name("unnamed"),
      isLocal(_isLocal),
      isGlobal(_isGlobal),
      isFixed(_isFixed),
      fake_object(false),
      isUserSpecified(false),
      allocSite(_allocSite) {
  }

  ~MemoryObject();

  /// Get an identifying string for this allocation.
  void getAllocInfo(std::string &result) const;

  void setName(std::string name) const {
    this->name = name;
  }

  ref<ConstantExpr> getBaseExpr() const { 
    return ConstantExpr::create(address, Context::get().getPointerWidth());
  }
  ref<ConstantExpr> getSizeExpr() const { 
    return ConstantExpr::create(size, Context::get().getPointerWidth());
  }
  ref<Expr> getOffsetExpr(ref<Expr> pointer) const {
    return SubExpr::create(pointer, getBaseExpr());
  }
  ref<Expr> getBoundsCheckPointer(ref<Expr> pointer) const {
    return getBoundsCheckOffset(getOffsetExpr(pointer));
  }
  ref<Expr> getBoundsCheckPointer(ref<Expr> pointer, unsigned bytes) const {
    return getBoundsCheckOffset(getOffsetExpr(pointer), bytes);
  }

  ref<Expr> getBoundsCheckOffset(ref<Expr> offset) const {
    if (size==0) {
      return EqExpr::create(offset, 
                            ConstantExpr::alloc(0, Context::get().getPointerWidth()));
    } else {
      return UltExpr::create(offset, getSizeExpr());
    }
  }
  ref<Expr> getBoundsCheckOffset(ref<Expr> offset, unsigned bytes) const {
    if (bytes<=size) {
      return UltExpr::create(offset, 
                             ConstantExpr::alloc(size - bytes + 1, 
                                                 Context::get().getPointerWidth()));
    } else {
      return ConstantExpr::alloc(0, Expr::Bool);
    }
  }
};

struct MemoryLogEntry {
  MemoryLogEntry() : threadId(0), wgid(0), read(0), write(0), manyRead(0),
                     wgManyRead(0) {}

  // The id of the thread which initially read from or wrote to this
  // memory location.
  unsigned threadId : 32;

  // The workgroup id of the thread which initially read from or wrote to this
  // memory location.
  unsigned wgid : 28;

  // In the following, a matching thread is a thread whose id == threadId,
  // or whose workgroup id == wgid.  A mismatching thread is a thread which
  // does not meet these criteria.  i.e. the matches function below.
  bool matches(unsigned otherThreadId, unsigned otherWgid) const {
    return threadId == otherThreadId || wgid == otherWgid;
  }

  // Set if a read is issued on this memory location.  A write from a
  // mismatching thread (or if manyRead is set, any write) will
  // trigger a race diagnostic.
  unsigned read : 1;

  // Set if a write is issued on this memory location.  A read or write from a
  // mismatching thread will trigger a race diagnostic.
  unsigned write : 1;

  // Set if read=1 and a thread whose id != threadId reads from this memory
  // location.
  unsigned manyRead : 1;

  // Set if read=1 and a thread whose workgroup id != wgid reads from this
  // memory location.
  unsigned wgManyRead : 1;

};

struct MemoryLogUpdates {
  MemoryLogUpdates() : threadId(0, 0), wgid(0, 0), read(0, 0), write(0, 0),
                       manyRead(0, 0), wgManyRead(0, 0) {}

  UpdateList threadId, wgid, read, write, manyRead, wgManyRead;
};

struct MemoryRace {
  enum RaceType {
    RT_readwrite, RT_writewrite
  };

  RaceType raceType;
  thread_id_t op1ThreadId, op2ThreadId;
};

class MemoryLog {
  static unsigned logId;

public:
  MemoryLog(unsigned size);
  MemoryLog(const MemoryLog &that);
  ~MemoryLog();

  unsigned size;
  std::vector<MemoryLogEntry> concreteEntries;
  MemoryLogUpdates *updates;

  bool isSymbolic() const { return updates; }
  void makeSymbolic();

  bool logRead(ExecutionState *state, TimingSolver *solver, unsigned offset, MemoryRace &raceInfo);
  bool logWrite(ExecutionState *state, TimingSolver *solver, unsigned offset, MemoryRace &raceInfo);

  bool logRead(ExecutionState *state, TimingSolver *solver, ref<Expr> offset, MemoryRace &raceInfo);
  bool logWrite(ExecutionState *state, TimingSolver *solver, ref<Expr> offset, MemoryRace &raceInfo);

  void localReset();
  void globalReset();
};

class ObjectState {
private:
  friend class AddressSpace;
  unsigned copyOnWriteOwner; // exclusively for AddressSpace

  friend class ObjectHolder;
  unsigned refCount;

  const MemoryObject *object;

  uint8_t *concreteStore;
  // XXX cleanup name of flushMask (its backwards or something)
  BitArray *concreteMask;

  // mutable because may need flushed during read of const
  mutable BitArray *flushMask;

  ref<Expr> *knownSymbolics;

  // mutable because we may need flush during read of const
  mutable UpdateList updates;

  // mutable because we log reads as well as writes
  mutable MemoryLog memoryLog;

public:
  unsigned size;

  bool readOnly;

  bool isShared; // The object is shared among addr. spaces within the same state

public:
  /// Create a new object state for the given memory object with concrete
  /// contents. The initial contents are undefined, it is the callers
  /// responsibility to initialize the object contents appropriately.
  ObjectState(const MemoryObject *mo);

  /// Create a new object state for the given memory object with symbolic
  /// contents.
  ObjectState(const MemoryObject *mo, const Array *array);

  ObjectState(const ObjectState &os);
  ~ObjectState();

  const MemoryObject *getObject() const { return object; }

  void setReadOnly(bool ro) { readOnly = ro; }

  // make contents all concrete and zero
  void initializeToZero();
  // make contents all concrete and random
  void initializeToRandom();

  ref<Expr> read(ref<Expr> offset, Expr::Width width, ExecutionState *state = 0, TimingSolver *solver = 0) const;
  ref<Expr> read(unsigned offset, Expr::Width width, ExecutionState *state = 0, TimingSolver *solver = 0) const;
  ref<Expr> read8(unsigned offset, ExecutionState *state = 0, TimingSolver *solver = 0) const;

  // return bytes written.
  void write(unsigned offset, ref<Expr> value, ExecutionState *state = 0, TimingSolver *solver = 0);
  void write(ref<Expr> offset, ref<Expr> value, ExecutionState *state = 0, TimingSolver *solver = 0);

  void write8(unsigned offset, uint8_t value, ExecutionState *state = 0, TimingSolver *solver = 0);
  void write16(unsigned offset, uint16_t value, ExecutionState *state = 0, TimingSolver *solver = 0);
  void write32(unsigned offset, uint32_t value, ExecutionState *state = 0, TimingSolver *solver = 0);
  void write64(unsigned offset, uint64_t value, ExecutionState *state = 0, TimingSolver *solver = 0);

  void globalResetMemoryLog();
  void localResetMemoryLog();

private:
  const UpdateList &getUpdates() const;

  void makeConcrete();

  void makeSymbolic();

  ref<Expr> read8(ref<Expr> offset, ExecutionState *state = 0, TimingSolver *solver = 0) const;
  void write8(unsigned offset, ref<Expr> value, ExecutionState *state = 0, TimingSolver *solver = 0);
  void write8(ref<Expr> offset, ref<Expr> value, ExecutionState *state = 0, TimingSolver *solver = 0);

  void fastRangeCheckOffset(ref<Expr> offset, unsigned *base_r, 
                            unsigned *size_r) const;
  void flushRangeForRead(unsigned rangeBase, unsigned rangeSize) const;
  void flushRangeForWrite(unsigned rangeBase, unsigned rangeSize);

  bool isByteConcrete(unsigned offset) const;
  bool isByteFlushed(unsigned offset) const;
  bool isByteKnownSymbolic(unsigned offset) const;

  void markByteConcrete(unsigned offset);
  void markByteSymbolic(unsigned offset);
  void markByteFlushed(unsigned offset);
  void markByteUnflushed(unsigned offset);
  void setKnownSymbolic(unsigned offset, Expr *value);

  void print();
};
  
} // End klee namespace

#endif
