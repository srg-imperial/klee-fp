#include <string.h>

#include <CL/cl.h>

#include <klee/Internal/CL/clintern.h>

cl_program clCreateProgramWithSource(cl_context context,
                                     cl_uint count,
                                     const char **strings,
                                     const size_t *lengths,
                                     cl_int *errcode_ret) {
  cl_program prog = malloc(sizeof(struct _cl_program));
  size_t progBufLen = 0;
  size_t *realLengths = malloc(sizeof(size_t) * count);
  unsigned i;
  char *source;

  for (i = 0; i < count; ++i) {
    size_t realLength = (lengths && lengths[i]) ? lengths[i] : strlen(strings[i]);
    progBufLen += realLengths[i] = realLength;
  }

  prog->source = source = malloc(progBufLen + 1);
  prog->sourceSize = progBufLen;
  prog->module = 0;
  source[progBufLen] = 0;
  for (i = count; i > 0; --i) {
    progBufLen -= realLengths[i-1];
    memcpy(source+progBufLen, strings[i-1], realLengths[i-1]);
  }
  
  if (errcode_ret)
    *errcode_ret = CL_SUCCESS;

  free(realLengths);

  return prog;
}
