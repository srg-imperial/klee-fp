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

  if ((flags & (CL_MEM_ALLOC_HOST_PTR | CL_MEM_USE_HOST_PTR)) ==
               (CL_MEM_ALLOC_HOST_PTR | CL_MEM_USE_HOST_PTR) ||
      (flags & (CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR)) ==
               (CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR)) {
    if (errcode_ret)
      *errcode_ret = CL_INVALID_VALUE;
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
    mem->ownsData = 0;
  }

  if (flags & (CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR)) {
    mem->data = malloc(size);
    mem->ownsData = 1;
    if (flags & CL_MEM_COPY_HOST_PTR)
      memcpy(mem->data, host_ptr, size);
  }

  if (errcode_ret)
    *errcode_ret = CL_SUCCESS;
  return mem;
}

cl_int clEnqueueReadBuffer(cl_command_queue command_queue,
                           cl_mem buffer,
                           cl_bool blocking_read,
                           size_t offset,
                           size_t cb,
                           void *ptr,
                           cl_uint num_events_in_wait_list,
                           const cl_event *event_wait_list,
                           cl_event *event) {
  if (!buffer)
    return CL_INVALID_MEM_OBJECT;

  if (offset + cb > buffer->size || !ptr)
    return CL_INVALID_VALUE;

  memcpy(ptr, buffer->data+offset, cb);

  return CL_SUCCESS;
}

cl_int clEnqueueWriteBuffer(cl_command_queue command_queue,
                            cl_mem buffer,
                            cl_bool blocking_write,
                            size_t offset,
                            size_t cb,
                            const void *ptr,
                            cl_uint num_events_in_wait_list,
                            const cl_event *event_wait_list,
                            cl_event *event) {
  if (!buffer)
    return CL_INVALID_MEM_OBJECT;

  if (offset + cb > buffer->size || !ptr)
    return CL_INVALID_VALUE;

  memcpy(buffer->data+offset, ptr, cb);

  return CL_SUCCESS;
}
