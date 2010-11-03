#include <string.h>

#include <CL/cl.h>

#include <klee/klee.h>
#include <klee/Internal/CL/clintern.h>

cl_mem clCreateBuffer(cl_context context,
                      cl_mem_flags flags,
                      size_t size,
                      void *host_ptr,
                      cl_int *errcode_ret) {
  if (size == 0) {
    if (errcode_ret)
      *errcode_ret = CL_INVALID_BUFFER_SIZE;
    return NULL;
  }

  if (!!host_ptr ^ !!(flags & (CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR))) {
    if (errcode_ret)
      *errcode_ret = CL_INVALID_HOST_PTR;
    return NULL;
  }

  cl_mem mem = malloc(sizeof(struct _cl_mem));
  mem->size = size;

  if (flags & CL_MEM_USE_HOST_PTR) {
    mem->data = host_ptr;
  }

  if (flags & (CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR)) {
    mem->data = malloc(size);
    if (flags & CL_MEM_COPY_HOST_PTR)
      memcpy(mem->data, host_ptr, size);
  }

  if (errcode_ret)
    *errcode_ret = CL_SUCCESS;
  return mem;
}
