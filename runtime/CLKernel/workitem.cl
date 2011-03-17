#include <klee/clkernel.h>

/* These data items are filled in dynamically by the host code
 * (see CLHost/kernel.c).  They are global to the NDRange. */
uint _work_dim;
size_t _global_work_offset[64];
size_t _global_work_size[64];
size_t _num_groups[64];

/* Array of global ids (not offset by the global work offsets).  Filled in
 * dynamically by the host code (see CLHost/kernel.c).  Each work item has
 * its own id list. */
__klee_thrlocal size_t _ids[64];

uint get_work_dim(void) {
  return _work_dim;
}

size_t get_global_size(uint dimindx) {
  return _global_work_size[dimindx];
}

size_t get_global_id(uint dimindx) {
  return _global_work_offset[dimindx] + _ids[dimindx];
}

size_t get_local_size(uint dimindx) {
  return _global_work_size[dimindx] / _num_groups[dimindx];
}

size_t get_local_id(uint dimindx) {
  return _ids[dimindx] % get_local_size(dimindx);
}

size_t get_num_groups(uint dimindx) {
  return _num_groups[dimindx];
}

size_t get_group_id(uint dimindx) {
  return _ids[dimindx] / get_local_size(dimindx);
}

size_t get_global_offset(uint dimindx) {
  return _global_work_offset[dimindx];
}
