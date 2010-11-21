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
 * AddressPool.h
 *
 *  Created on: Sep 10, 2010
 *  File owner: Stefan Bucur <stefan.bucur@epfl.ch>
 ******************************************************************************/

#ifndef ADDRESSPOOL_H_
#define ADDRESSPOOL_H_

#define ADDRESS_POOL_START      0xDEADBEEF00000000L // XXX This may very well overlap a mmap()-ed region
#define ADDRESS_POOL_SIZE       0x10000000
#define ADDRESS_POOL_GAP        (4*sizeof(uint64_t))
#define ADDRESS_POOL_ALIGN      (4*sizeof(uint64_t)) // Must be a power of two

namespace klee {

class AddressPool {
private:
  uint64_t startAddress;
  uint64_t size;


  uint64_t currentAddress;

  uint64_t gap;
  uint64_t align; // Must be a power of two
public:
  AddressPool(uint64_t _start, uint64_t _size) :
    startAddress(_start), size(_size),
    currentAddress(startAddress),
    gap(ADDRESS_POOL_GAP), align(ADDRESS_POOL_ALIGN) { }

  AddressPool() :
    startAddress(ADDRESS_POOL_START), size(ADDRESS_POOL_SIZE),
    currentAddress(startAddress),
    gap(ADDRESS_POOL_GAP), align(ADDRESS_POOL_ALIGN) { }

  virtual ~AddressPool() { }

  uint64_t allocate(uint64_t amount) {
    if (currentAddress + amount > startAddress + size)
      return 0;

    uint64_t result = currentAddress;

    currentAddress += amount + gap;

    if ((currentAddress & (align - 1)) != 0) {
      currentAddress = (currentAddress + align) & (~(align - 1));
    }

    return result;
  }

  uint64_t getStartAddress() const { return startAddress; }
  uint64_t getSize() const { return size; }
};

}

#endif /* ADDRESSPOOL_H_ */
