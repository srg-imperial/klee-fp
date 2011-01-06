#include <klee/klee.h>
#include <klee/clkernel.h>

__klee_thrlocal uint64_t _wg_barrier_wlist;
unsigned _wg_barrier_size;

void mem_fence(cl_mem_fence_flags flags) {
  if (flags & CLK_LOCAL_MEM_FENCE)
    klee_thread_barrier(_wg_barrier_wlist, _wg_barrier_size, 1);
  if (flags & CLK_GLOBAL_MEM_FENCE)
    klee_thread_barrier(_wg_barrier_wlist, _wg_barrier_size, 0);
}
