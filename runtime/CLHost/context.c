#include <string.h>

#include <CL/cl.h>

#include <klee/klee.h>
#include <klee/Internal/CL/clintern.h>

static cl_context create_the_context(
                           void (CL_CALLBACK *pfn_notify)(const char *errinfo,
                                             const void *private_info, size_t cb,
                                             void *user_data),
                           void *user_data) {
  cl_context ctx = malloc(sizeof(struct _cl_context));
  ctx->pfn_notify = pfn_notify;
  ctx->user_data = user_data;
  return ctx;
}

cl_context clCreateContext(const cl_context_properties *properties,
                           cl_uint num_devices,
                           const cl_device_id *devices,
                           void (CL_CALLBACK *pfn_notify)(const char *errinfo,
                                             const void *private_info, size_t cb,
                                             void *user_data),
                           void *user_data,
                           cl_int *errcode_ret) {
  if (devices == NULL || num_devices == 0) {
    if (errcode_ret)
      *errcode_ret = CL_INVALID_VALUE;
    return NULL;
  }

  return create_the_context(pfn_notify, user_data);
}

cl_context
clCreateContextFromType(const cl_context_properties *properties,
                        cl_device_type device_type,
                        void (CL_CALLBACK *pfn_notify)(const char *errinfo,
                                          const void *private_info, size_t cb,
                                          void *user_data),
                        void *user_data,
                        cl_int *errcode_ret) {
  if ((device_type & (CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_DEFAULT)) == 0) { 
    if (errcode_ret)
      *errcode_ret = CL_DEVICE_NOT_FOUND;
    return NULL;
  }

  return create_the_context(pfn_notify, user_data);
}
