#include <string.h>

#include <CL/cl.h>

#include <klee/klee.h>
#include <klee/Internal/CL/clintern.h>

cl_int clWaitForEvents(cl_uint num_events, const cl_event *event_list) {
  return CL_SUCCESS;
}

cl_int clReleaseEvent(cl_event event) {
  return CL_SUCCESS;
}
