#include <string.h>
#include <pthread.h>

#include <CL/cl.h>

#include <klee/klee.h>
#include <klee/Internal/CL/clintern.h>

cl_event create_pthread_event(pthread_t *threads, size_t threadCount) {
  cl_event event = malloc(sizeof(struct _cl_event));
  event->refCount = 1;
  event->threads = threads;
  event->threadCount = threadCount;
  return event;
}

static cl_int wait_for_event(cl_event event) {
  unsigned i;
  if (!event)
    return CL_SUCCESS;
  for (i = 0; i < event->threadCount; ++i) {
    pthread_join(event->threads[i], 0);
  }
  return CL_SUCCESS;
}

cl_int clWaitForEvents(cl_uint num_events, const cl_event *event_list) {
  unsigned i;
  for (i = 0; i < num_events; ++i) {
    int rv = wait_for_event(event_list[i]);
    if (rv != CL_SUCCESS)
      return rv;
  }

  return CL_SUCCESS;
}

cl_int clReleaseEvent(cl_event event) {
  if (!event)
    return CL_INVALID_EVENT;

  if (--event->refCount == 0) {
    free(event->threads);
    free(event);
  }

  return CL_SUCCESS;
}
