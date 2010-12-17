#include <klee/klee.h>
#include <klee/clkernel.h>

void mem_fence (cl_mem_fence_flags flags) {
  klee_thread_preempt(1);
}
