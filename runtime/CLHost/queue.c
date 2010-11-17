#include <string.h>

#include <CL/cl.h>

#include <klee/klee.h>
#include <klee/Internal/CL/clintern.h>

cl_command_queue clCreateCommandQueue(cl_context context,
                                      cl_device_id device,
                                      cl_command_queue_properties properties,
                                      cl_int *errcode_ret) {
  cl_int errcode = clRetainContext(context);
  if (errcode_ret)
    *errcode_ret = errcode;
  if (errcode != CL_SUCCESS)
    return NULL;

  cl_command_queue queue = malloc(sizeof(struct _cl_command_queue));
  queue->refCount = 1;
  queue->context = context;
  return queue;
}

cl_int clRetainCommandQueue(cl_command_queue command_queue) {
  if (!command_queue)
    return CL_INVALID_COMMAND_QUEUE;

  ++command_queue->refCount;

  return CL_SUCCESS;
}

cl_int clReleaseCommandQueue(cl_command_queue command_queue) {
  if (!command_queue)
    return CL_INVALID_COMMAND_QUEUE;

  if (--command_queue->refCount == 0) {
    clReleaseContext(command_queue->context);
    free(command_queue);
  }

  return CL_SUCCESS;
}
