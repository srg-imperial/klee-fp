#include <string.h>

#include <CL/cl.h>

#include <klee/klee.h>
#include <klee/Internal/CL/clintern.h>

cl_int clGetPlatformIDs(cl_uint num_entries,
                        cl_platform_id *platforms,
                        cl_uint *num_platforms) {
  if ((num_entries == 0 && platforms != NULL)
   || (num_platforms != NULL && platforms != NULL))
    return CL_INVALID_VALUE;

  if (platforms)
    platforms[0] = (cl_platform_id) 1;

  if (num_platforms)
    *num_platforms = 1;

  return CL_SUCCESS;
}

cl_int clGetPlatformInfo(cl_platform_id platform,
                         cl_platform_info param_name,
                         size_t param_value_size,
                         void *param_value,
                         size_t *param_value_size_ret) {
  const char *rs;

  if (platform != (cl_platform_id) 1)
    return CL_INVALID_PLATFORM;

  switch (param_name) {
    case CL_PLATFORM_PROFILE:    rs = "FULL_PROFILE"; break;
    case CL_PLATFORM_VERSION:    rs = "OpenCL 1.1 "; break;
    case CL_PLATFORM_NAME:       rs = "KLEE Symbolic Virtual Machine"; break;
    case CL_PLATFORM_VENDOR:     rs = "LLVM Project"; break;
    case CL_PLATFORM_EXTENSIONS: rs = ""; break;
    default: return CL_INVALID_VALUE;
  }

  if (param_value) {
    char *param_str = param_value;
    strncpy(param_str, rs, param_value_size-1);
    param_str[param_value_size-1] = 0;
  }

  if (param_value_size_ret)
    *param_value_size_ret = strlen(rs) + 1;

  return CL_SUCCESS;
}
