#include <stddef.h>
#include <stdint.h>

struct _cl_program {
  char *source;
  size_t sourceSize;
  uintptr_t module;
};

struct _cl_kernel {
  uintptr_t function;
};
