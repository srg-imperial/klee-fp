#include <stddef.h>
#include <stdint.h>

#include <CL/cl.h>

struct _cl_context {
  void (CL_CALLBACK *pfn_notify)(const char *errinfo,
               const void *private_info, size_t cb,
               void *user_data);
  void *user_data;
};

struct _cl_program {
  char *source;
  size_t sourceSize;
  uintptr_t module;
};

struct _cl_kernel {
  uintptr_t function;
};

struct _cl_command_queue {
  struct _cl_context *context;
};

struct _cl_mem {
  void *data;
  size_t size;
};
typedef int8_t cl_intern_arg_type;

#define CL_INTERN_ARG_TYPE_I8  0
#define CL_INTERN_ARG_TYPE_I16 1
#define CL_INTERN_ARG_TYPE_I32 2
#define CL_INTERN_ARG_TYPE_I64 3
#define CL_INTERN_ARG_TYPE_F32 4
#define CL_INTERN_ARG_TYPE_F64 5
#define CL_INTERN_ARG_TYPE_MEM 6
