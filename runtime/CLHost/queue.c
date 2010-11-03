#include <string.h>

#include <CL/cl.h>

#include <klee/klee.h>
#include <klee/Internal/CL/clintern.h>

cl_command_queue clCreateCommandQueue(cl_context context,
                                      cl_device_id device,
                                      cl_command_queue_properties properties,
                                      cl_int *errcode_ret) {
  cl_command_queue queue = malloc(sizeof(struct _cl_command_queue));
  queue->context = context;
  if (errcode_ret)
    *errcode_ret = CL_SUCCESS;
  return queue;
}
