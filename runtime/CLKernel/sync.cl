#include <klee/klee.h>
#include <klee/clkernel.h>

__klee_thrlocal uint64_t _wg_barrier_wlist;
unsigned _wg_barrier_size;

void barrier(cl_mem_fence_flags flags) {
  if (flags & CLK_LOCAL_MEM_FENCE)
    klee_thread_barrier(_wg_barrier_wlist, _wg_barrier_size, /*addrspace=*/1, /*isglobal=*/0);
  if (flags & CLK_GLOBAL_MEM_FENCE)
    klee_thread_barrier(_wg_barrier_wlist, _wg_barrier_size, /*addrspace=*/0, /*isglobal=*/0);
}
