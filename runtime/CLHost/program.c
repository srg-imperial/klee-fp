#include <string.h>

#include <CL/cl.h>

#include <klee/klee.h>
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

  prog->refCount = 1;
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

cl_int clBuildProgram(cl_program program,
                      cl_uint num_devices,
                      const cl_device_id *device_list,
                      const char *options,
                      void (CL_CALLBACK *pfn_notify)(cl_program program,
                                                     void *user_data),
                      void *user_data) {
  cl_int result;

  program->module = klee_ocl_compile(program->source, options);
  result = program->module ? CL_SUCCESS : CL_BUILD_PROGRAM_FAILURE;

  if (program->module) {
    program->localIds = (size_t *) klee_lookup_module_global(program->module, "_local_ids");
    program->globalIds = (size_t *) klee_lookup_module_global(program->module, "_global_ids");
  }

  if (pfn_notify)
    pfn_notify(program, user_data);

  return result;
}

cl_int clRetainProgram(cl_program program) {
  if (!program)
    return CL_INVALID_PROGRAM;

  ++program->refCount;

  return CL_SUCCESS;
}

cl_int clReleaseProgram(cl_program program) {
  if (!program)
    return CL_INVALID_PROGRAM;

  if (--program->refCount == 0) {
    free(program->source);
    free(program);
  }

  return CL_SUCCESS;
}
