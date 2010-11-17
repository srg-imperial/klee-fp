#include <string.h>

#include <CL/cl.h>

#include <klee/klee.h>
#include <klee/Internal/CL/clintern.h>

cl_kernel clCreateKernel(cl_program program,
                         const char *kernel_name,
                         cl_int *errcode_ret) {
  void (*function)() = (void (*)()) klee_lookup_module_global(program->module, kernel_name);

  if (errcode_ret)
    *errcode_ret = function ? CL_SUCCESS : CL_INVALID_KERNEL_NAME;

  if (function) {
    cl_int errcode = clRetainProgram(program);
    if (errcode != CL_SUCCESS) {
      if (*errcode_ret)
        *errcode_ret = errcode;
      return NULL;
    }

    cl_kernel kernel = malloc(sizeof(struct _cl_kernel));
    kernel->refCount = 1;
    kernel->program = program;
    kernel->function = function;
    return kernel;
  } else {
    return NULL;
  }
}

cl_int clSetKernelArg(cl_kernel kernel,
                      cl_uint arg_index,
                      size_t arg_size,
                      const void *arg_value) {
  if (arg_index >= klee_ocl_get_arg_count(kernel->function))
    return CL_INVALID_ARG_INDEX;

  switch (klee_ocl_get_arg_type(kernel->function, arg_index)) {
#define X(TYPE, FIELD) { \
      const TYPE *ap = arg_value; \
 \
      if (!arg_value) \
        kernel->args[arg_index].FIELD = 0; \
 \
      if (arg_size != sizeof(TYPE)) \
        return CL_INVALID_ARG_SIZE; \
 \
      kernel->args[arg_index].FIELD = *ap; \
      break; \
    }
    case CL_INTERN_ARG_TYPE_I8: X(uint8_t, i8)
    case CL_INTERN_ARG_TYPE_I16: X(uint16_t, i16)
    case CL_INTERN_ARG_TYPE_I32: X(uint32_t, i32)
    case CL_INTERN_ARG_TYPE_I64: X(uint64_t, i64)
    case CL_INTERN_ARG_TYPE_F32: X(float, f32)
    case CL_INTERN_ARG_TYPE_F64: X(double, f64)
    case CL_INTERN_ARG_TYPE_MEM: {
      if (arg_size != sizeof(cl_mem))
        return CL_INVALID_ARG_SIZE;

      if (!arg_value || !*(cl_mem *)arg_value) {
        kernel->args[arg_index].mem.data = NULL;
        kernel->args[arg_index].mem.size = 0;
      } else {
        cl_mem ap = * (cl_mem *) arg_value;
        kernel->args[arg_index].mem = *ap;
      }
      break;
    }
    default: return CL_INVALID_KERNEL;
#undef X
  }

  return CL_SUCCESS;
}

cl_int clEnqueueNDRangeKernel(cl_command_queue command_queue,
                              cl_kernel kernel,
                              cl_uint work_dim,
                              const size_t *global_work_offset,
                              const size_t *global_work_size,
                              const size_t *local_work_size,
                              cl_uint num_events_in_wait_list,
                              const cl_event *event_wait_list,
                              cl_event *event) {
  uintptr_t argList = klee_icall_create_arg_list();
  unsigned argCount = klee_ocl_get_arg_count(kernel->function);
  unsigned i;

  for (i = 0; i < argCount; ++i) {
    switch (klee_ocl_get_arg_type(kernel->function, i)) {
#define X(TYPE, FIELD) { \
        TYPE a = kernel->args[i].FIELD; \
        klee_icall_add_arg(argList, &a, sizeof(a)); \
        break; \
      }
      case CL_INTERN_ARG_TYPE_I8: X(uint8_t, i8)
      case CL_INTERN_ARG_TYPE_I16: X(uint16_t, i16)
      case CL_INTERN_ARG_TYPE_I32: X(uint32_t, i32)
      case CL_INTERN_ARG_TYPE_I64: X(uint64_t, i64)
      case CL_INTERN_ARG_TYPE_F32: X(float, f32)
      case CL_INTERN_ARG_TYPE_F64: X(double, f64)
      case CL_INTERN_ARG_TYPE_MEM: {
        void *a = kernel->args[i].mem.data;
        klee_icall_add_arg(argList, &a, sizeof(a));
        break;
      }
      default: return CL_INVALID_KERNEL;
    }
  }
  
  klee_icall(kernel->function, argList);
  klee_icall_destroy_arg_list(argList);

  return CL_SUCCESS;
}

cl_int clRetainKernel(cl_kernel kernel) {
  if (!kernel)
    return CL_INVALID_KERNEL;

  ++kernel->refCount;

  return CL_SUCCESS;
}

cl_int clReleaseKernel(cl_kernel kernel) {
  if (!kernel)
    return CL_INVALID_KERNEL;

  if (--kernel->refCount == 0) {
    clReleaseProgram(kernel->program);
    free(kernel);
  }

  return CL_SUCCESS;
}
