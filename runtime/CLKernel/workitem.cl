#include <klee/clkernel.h>

size_t _local_ids[64], _global_ids[64];

size_t get_global_id(uint dimindx) {
  return _global_ids[dimindx];
}

size_t get_local_id(uint dimindx) {
  return _local_ids[dimindx];
}
