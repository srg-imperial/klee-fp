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
