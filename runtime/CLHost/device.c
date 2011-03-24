#include <string.h>
#include <CL/cl.h>

cl_int clGetDeviceIDs(cl_platform_id platform,
                      cl_device_type device_type,
                      cl_uint num_entries,
                      cl_device_id *devices,
                      cl_uint *num_devices) {
  if (platform != (cl_platform_id) 1)
    return CL_INVALID_PLATFORM;

  if (num_entries == 0 && devices != 0)
    return CL_INVALID_VALUE;
  if (num_devices == 0 && devices == 0)
    return CL_INVALID_VALUE;

  if ((device_type & (CL_DEVICE_TYPE_DEFAULT & CL_DEVICE_TYPE_ALL)) == 0) {
    if (num_devices)
      *num_devices = 0;
    return CL_DEVICE_NOT_FOUND;
  } else {
    if (devices && num_entries > 0)
      *devices = (cl_device_id) 1;
    if (num_devices)
      *num_devices = 1;
    return CL_SUCCESS;
  }
}

static void get_max_work_item_sizes(cl_device_id device, cl_uint sizes[]) {
  unsigned i;
  for (i = 0; i != 64; ++i)
    sizes[i] = 65536;
}

cl_int clGetDeviceInfo(cl_device_id device,
                       cl_device_info param_name,
                       size_t param_value_size,
                       void *param_value,
                       size_t *param_value_size_ret) {
  if (device != (cl_device_id) 1)
    return CL_INVALID_DEVICE;

  switch (param_name) {
#define DEVICE_INFO_SIMPLE_KIND(info, type, value) \
  case info: \
    if (param_value_size_ret) \
      *param_value_size_ret = sizeof(type); \
    if (param_value) { \
      if (param_value_size < sizeof(type)) \
        return CL_INVALID_VALUE; \
     *(type *)param_value = value; \
    } \
    break;
#define DEVICE_INFO_ARRAY_KIND(info, type, size, fn) \
  case info: \
    if (param_value_size_ret) \
      *param_value_size_ret = sizeof(type)*size; \
    if (param_value) { \
      if (param_value_size < sizeof(type)*size) \
        return CL_INVALID_VALUE; \
     fn(device, (type *) param_value); \
    } \
    break;
#define DEVICE_INFO_STRING_KIND(info, str) \
  case info: \
    if (param_value_size_ret) \
      *param_value_size_ret = sizeof(str); \
    if (param_value) { \
      if (param_value_size < sizeof(str)) \
        return CL_INVALID_VALUE; \
      memcpy(param_value, str, sizeof(str)); \
    } \
    break;
#include "DeviceInfo.inc"
  default: return CL_INVALID_VALUE;
  }

  return CL_SUCCESS;
}
