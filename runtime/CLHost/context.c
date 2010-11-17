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
  ctx->refCount = 1;
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

cl_int clGetContextInfo(cl_context context,
                        cl_context_info param_name,
                        size_t param_value_size,
                        void *param_value,
                        size_t *param_value_size_ret) {
  switch (param_name) {
    case CL_CONTEXT_NUM_DEVICES: {
      if (param_value_size_ret)
        *param_value_size_ret = sizeof(cl_uint);
      if (param_value) {
        cl_uint *r = param_value;
        if (param_value_size < sizeof(cl_uint))
          return CL_INVALID_VALUE;
        *r = 1;
      }
      break;
    }
    case CL_CONTEXT_DEVICES: {
      if (param_value_size_ret)
        *param_value_size_ret = sizeof(cl_device_id);
      if (param_value) {
        cl_device_id *r = param_value;
        if (param_value_size < sizeof(cl_device_id))
          return CL_INVALID_VALUE;
        *r = (cl_device_id) 1;
      }
      break;
    }
    default: return CL_INVALID_VALUE;
  }

  return CL_SUCCESS;
}

cl_int clRetainContext(cl_context context) {
  if (!context)
    return CL_INVALID_CONTEXT;

  ++context->refCount;

  return CL_SUCCESS;
}

cl_int clReleaseContext(cl_context context) {
  if (!context)
    return CL_INVALID_CONTEXT;

  if (--context->refCount == 0) {
    free(context);
  }

  return CL_SUCCESS;
}
