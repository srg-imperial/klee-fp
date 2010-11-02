#include <string.h>

#include <CL/cl.h>

#include <klee/klee.h>
#include <klee/Internal/CL/clintern.h>

cl_kernel clCreateKernel(cl_program program,
                         const char *kernel_name,
                         cl_int *errcode_ret) {
  uintptr_t function = klee_ocl_lookup_kernel_function(program->module, kernel_name);

  if (errcode_ret)
    *errcode_ret = function ? CL_SUCCESS : CL_INVALID_KERNEL_NAME;

  if (function) {
    cl_kernel kernel = malloc(sizeof(struct _cl_kernel));
    kernel->function = function;
    return kernel;
  } else {
    return NULL;
  }
}
