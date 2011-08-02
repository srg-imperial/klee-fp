#include <string.h>
#include <pthread.h>

#include <CL/cl.h>

#include <klee/klee.h>
#include <klee/Internal/CL/clintern.h>

#undef DUMP_NDRANGE

#ifdef DUMP_NDRANGE
#include <stdio.h>
#endif

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
    case CL_INTERN_ARG_TYPE_LOCAL_MEM: {
      if (arg_value)
        return CL_INVALID_ARG_VALUE;
      if (arg_size == 0)
        return CL_INVALID_ARG_SIZE;

      kernel->args[arg_index].local_size = arg_size;
      break;
    }
    default: return CL_INVALID_KERNEL;
#undef X
  }

  return CL_SUCCESS;
}

static cl_uint increment_id_list(cl_uint work_dim, size_t *ids,
                             const size_t *sizes) {
  cl_uint i;
  for (i = work_dim; i > 0; --i) {
    size_t id = ids[i-1] = (ids[i-1] + 1) % sizes[i-1];
    if (id != 0)
      return i;
  }
  return 0;
}

typedef struct _cl_intern_work_item_params {
  cl_kernel kernel;
  uintptr_t args;
  cl_uint work_dim;
  unsigned wgid;
  uint64_t wg_wlist;
  unsigned wg_size;
  size_t ids[64];
} cl_intern_work_item_params;

static __attribute((address_space(4))) void *memcpy40(
  __attribute((address_space(4))) void *destaddr, void const *srcaddr,
  size_t len) {
  __attribute((address_space(4))) char *dest = destaddr;
  char const *src = srcaddr;

  while (len-- > 0)
    *dest++ = *src++;
  return destaddr;
}

static void *work_item_thread(void *arg) {
  cl_intern_work_item_params *params = arg;
  uintptr_t args = params->args;
  uint64_t wg_wlist = params->wg_wlist;
  unsigned wg_size = params->wg_size;
  cl_kernel kern = params->kernel;
  cl_program prog = kern->program;

  if (prog->ids) {
    memcpy40(prog->ids, params->ids, sizeof(size_t)*params->work_dim);
  }

  klee_set_work_group_id(params->wgid);
  if (prog->wgBarrierWlist)
    *prog->wgBarrierWlist = wg_wlist;

  free(arg);
  
  klee_icall(kern->function, args);

  klee_thread_barrier(wg_wlist, wg_size, 1);
  klee_thread_barrier(wg_wlist, wg_size, 0);

  return 0;
}

static int invoke_work_item(cl_kernel kern, uintptr_t args, cl_uint work_dim,
                            unsigned wgid, uint64_t wg_wlist, unsigned wg_size,
                            size_t ids[], pthread_t *pt) {
  cl_intern_work_item_params *params = malloc(sizeof(cl_intern_work_item_params));

  params->kernel = kern;
  params->args = args;
  params->work_dim = work_dim;
  params->wgid = wgid;
  params->wg_wlist = wg_wlist;
  params->wg_size = wg_size;
  memcpy(params->ids, ids, sizeof(size_t)*work_dim);

  return pthread_create(pt, NULL, work_item_thread, params);
}

#ifdef DUMP_NDRANGE
static void dump_ndrange_size(const char *name, cl_uint work_dim, const size_t *size) {
  printf("  %s ", name);
  fflush(stdout);
  if (size) {
    cl_uint dim;
    printf("(");
    if (work_dim > 0)
      printf("%lu", (unsigned long) size[0]);
    for (dim = 1; dim < work_dim; ++dim)
      printf(", %lu", (unsigned long) size[dim]);
    fflush(stdout);
    puts(")");
  } else {
    puts("NULL");
  }
}
#endif

cl_int clEnqueueNDRangeKernel(cl_command_queue command_queue,
                              cl_kernel kernel,
                              cl_uint work_dim,
                              const size_t *global_work_offset,
                              const size_t *global_work_size,
                              const size_t *local_work_size,
                              cl_uint num_events_in_wait_list,
                              const cl_event *event_wait_list,
                              cl_event *event) {
  size_t num_groups[64], ids[64], local_ids[64], global_ids[64], workgroup_count,
         work_item_count;
  cl_uint i, last_id;
  uintptr_t argList;
  unsigned *workgroups;
  uint64_t *wg_wlists;
  pthread_t *work_items, *cur_work_item;
  cl_event new_event;

#ifdef DUMP_NDRANGE
  puts("clEnqueueNDRangeKernel:");
  dump_ndrange_size("global_work_offset", work_dim, global_work_offset);
  dump_ndrange_size("global_work_size", work_dim, global_work_size);
  dump_ndrange_size("local_work_size", work_dim, local_work_size);
#endif

  int rv = clFinish(command_queue);
  if (rv != CL_SUCCESS)
    return rv;

  if (!global_work_size)
    return CL_INVALID_GLOBAL_WORK_SIZE;

  for (i = 0; i < work_dim; ++i) {
    if (global_work_size[i] == 0)
      return CL_INVALID_GLOBAL_WORK_SIZE;

    if (local_work_size) {
      if (local_work_size[i] == 0)
        return CL_INVALID_WORK_GROUP_SIZE;
      if (global_work_size[i] % local_work_size[i] != 0)
        return CL_INVALID_WORK_GROUP_SIZE;
      num_groups[i] = global_work_size[i] / local_work_size[i];
    } else {
      num_groups[i] = 1;
    }
  }

  memset(ids, 0, work_dim*sizeof(size_t));
  if (global_work_offset)
    memcpy(global_ids, global_work_offset, work_dim*sizeof(size_t));
  else
    memset(global_ids, 0, work_dim*sizeof(size_t));
  memset(local_ids, 0, work_dim*sizeof(size_t));
  last_id = work_dim+1;

  work_item_count = 1;
  for (i = 0; i < work_dim; ++i)
    work_item_count *= global_work_size[i];

  cur_work_item = work_items = malloc(sizeof(pthread_t) * work_item_count);

  workgroup_count = 1;
  if (local_work_size)
    for (i = 0; i < work_dim; ++i)
      workgroup_count *= num_groups[i];

  if (kernel->program->wgBarrierSize)
    *kernel->program->wgBarrierSize = work_item_count/workgroup_count;

  workgroups = malloc(sizeof(unsigned) * workgroup_count);
  for (i = 0; i < workgroup_count; ++i)
    workgroups[i] = klee_create_work_group();

  wg_wlists = malloc(sizeof(uint64_t) * workgroup_count);
  for (i = 0; i < workgroup_count; ++i)
    wg_wlists[i] = klee_get_wlist();

  {
    argList = klee_icall_create_arg_list();
    unsigned argCount = klee_ocl_get_arg_count(kernel->function);
    unsigned arg;

    for (arg = 0; arg < argCount; ++arg) {
      switch (klee_ocl_get_arg_type(kernel->function, arg)) {
#define X(TYPE, FIELD) { \
          TYPE a = kernel->args[arg].FIELD; \
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
          void *a = kernel->args[arg].mem.data;
          klee_icall_add_arg(argList, &a, sizeof(a));
          break;
        }
        case CL_INTERN_ARG_TYPE_LOCAL_MEM: {
          __attribute__((address_space(1))) void *a =
            klee_asmalloc(1)(kernel->args[arg].local_size);
          klee_icall_add_arg(argList, &a, sizeof(a));
          break;
        }
        default: return CL_INVALID_KERNEL;
      }
    }
  }

  if (kernel->program->workDim) {
    *kernel->program->workDim = work_dim;
  }

  if (kernel->program->globalWorkOffset) {
    if (global_work_offset)
      memcpy(kernel->program->globalWorkOffset, global_work_offset, work_dim*sizeof(size_t));
    else
      memset(kernel->program->globalWorkOffset, 0, work_dim*sizeof(size_t));
  }

  if (kernel->program->globalWorkSize) {
    memcpy(kernel->program->globalWorkSize, global_work_size, work_dim*sizeof(size_t));
  }

  if (kernel->program->numGroups) {
    memcpy(kernel->program->numGroups, num_groups, work_dim*sizeof(size_t));
  }

  do {
    /* Build up a one-dimensional work-group id for the work item.
     * Each iteration multiplies the current id by the number of
     * groups (to scale the value against the current dimension)
     * and then adds the group id for that dimension (which is
     * guaranteed to be between 0 and num_groups-1).
     */
    unsigned wgid = 0;
    if (local_work_size)
      for (i = 0; i < work_dim; ++i)
        wgid = wgid*num_groups[i] + (ids[i] / local_work_size[i]);

    invoke_work_item(kernel, argList, work_dim, workgroups[wgid],
        wg_wlists[wgid], work_item_count/workgroup_count, ids, cur_work_item++);
  } while ((last_id = increment_id_list(work_dim, ids, global_work_size)));

  new_event = kcl_create_pthread_event(work_items, work_item_count);
  kcl_add_event_to_queue(command_queue, new_event);

  if (event) {
    clRetainEvent(new_event);
    *event = new_event;
  }

  // klee_icall_destroy_arg_list(argList);

  return CL_SUCCESS;
}

cl_int clEnqueueTask(cl_command_queue command_queue,
                     cl_kernel kernel,
                     cl_uint num_events_in_wait_list,
                     const cl_event *event_wait_list,
                     cl_event *event) {
  size_t one = 1;
  return clEnqueueNDRangeKernel(command_queue,
                                kernel,
                                1,
                                NULL,
                                &one,
                                NULL,
                                num_events_in_wait_list,
                                event_wait_list,
                                event);
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
