#include <stddef.h>

#define __local __attribute__((address_space(1)))
#define __constant
#define __global
#define __private
#define __klee_thrlocal __attribute__((address_space(4)))

#define __kernel

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

typedef __attribute__((ext_vector_type(2))) char char2;
typedef __attribute__((ext_vector_type(3))) char char3;
typedef __attribute__((ext_vector_type(4))) char char4;
typedef __attribute__((ext_vector_type(8))) char char8;
typedef __attribute__((ext_vector_type(16))) char char16;

typedef __attribute__((ext_vector_type(2))) uchar uchar2;
typedef __attribute__((ext_vector_type(3))) uchar uchar3;
typedef __attribute__((ext_vector_type(4))) uchar uchar4;
typedef __attribute__((ext_vector_type(8))) uchar uchar8;
typedef __attribute__((ext_vector_type(16))) uchar uchar16;

typedef __attribute__((ext_vector_type(2))) short short2;
typedef __attribute__((ext_vector_type(3))) short short3;
typedef __attribute__((ext_vector_type(4))) short short4;
typedef __attribute__((ext_vector_type(8))) short short8;
typedef __attribute__((ext_vector_type(16))) short short16;

typedef __attribute__((ext_vector_type(2))) ushort ushort2;
typedef __attribute__((ext_vector_type(3))) ushort ushort3;
typedef __attribute__((ext_vector_type(4))) ushort ushort4;
typedef __attribute__((ext_vector_type(8))) ushort ushort8;
typedef __attribute__((ext_vector_type(16))) ushort ushort16;

typedef __attribute__((ext_vector_type(2))) int int2;
typedef __attribute__((ext_vector_type(3))) int int3;
typedef __attribute__((ext_vector_type(4))) int int4;
typedef __attribute__((ext_vector_type(8))) int int8;
typedef __attribute__((ext_vector_type(16))) int int16;

typedef __attribute__((ext_vector_type(2))) uint uint2;
typedef __attribute__((ext_vector_type(3))) uint uint3;
typedef __attribute__((ext_vector_type(4))) uint uint4;
typedef __attribute__((ext_vector_type(8))) uint uint8;
typedef __attribute__((ext_vector_type(16))) uint uint16;

typedef __attribute__((ext_vector_type(2))) long long2;
typedef __attribute__((ext_vector_type(3))) long long3;
typedef __attribute__((ext_vector_type(4))) long long4;
typedef __attribute__((ext_vector_type(8))) long long8;
typedef __attribute__((ext_vector_type(16))) long long16;

typedef __attribute__((ext_vector_type(2))) ulong ulong2;
typedef __attribute__((ext_vector_type(3))) ulong ulong3;
typedef __attribute__((ext_vector_type(4))) ulong ulong4;
typedef __attribute__((ext_vector_type(8))) ulong ulong8;
typedef __attribute__((ext_vector_type(16))) ulong ulong16;

typedef __attribute__((ext_vector_type(2))) float float2;
typedef __attribute__((ext_vector_type(3))) float float3;
typedef __attribute__((ext_vector_type(4))) float float4;
typedef __attribute__((ext_vector_type(8))) float float8;
typedef __attribute__((ext_vector_type(16))) float float16;

/* 6.11.1 Work-Item Functions */
uint get_work_dim(void);
size_t get_global_size(uint dimindx);
size_t get_global_id(uint dimindx);
size_t get_local_size(uint dimindx);
size_t get_local_id(uint dimindx);
size_t get_num_groups(uint dimindx);
size_t get_group_id(uint dimindx);
size_t get_global_offset(uint dimindx);

/* 6.11.2 Math Functions */

#define DECLARE_NATIVE_DIVIDE(__gentype) \
__attribute__((overloadable)) __gentype native_divide(__gentype x, __gentype y);

DECLARE_NATIVE_DIVIDE(float)
DECLARE_NATIVE_DIVIDE(float2)
DECLARE_NATIVE_DIVIDE(float3)
DECLARE_NATIVE_DIVIDE(float4)
DECLARE_NATIVE_DIVIDE(float8)
DECLARE_NATIVE_DIVIDE(float16)

float native_cos(float);
float native_sin(float);
float native_sqrt(float);

/* 6.11.5 Geometric Functions */

#define cross(p0, p1) _Generic((p0), \
  float3: (float3)((p0).y*(p1).z - (p0).z*(p1).y, (p0).z*(p1).x - (p0).x*(p1).z, (p0).x*(p1).y - (p0).y*(p1).x), \
  float4: (float4)((p0).y*(p1).z - (p0).z*(p1).y, (p0).z*(p1).x - (p0).x*(p1).z, (p0).x*(p1).y - (p0).y*(p1).x, 0.f))

float __dot1(float p0, float p1);
float __dot2(float2 p0, float2 p1);
float __dot3(float3 p0, float3 p1);
float __dot4(float4 p0, float4 p1);

#define dot(p0, p1) _Generic((p0), \
  float: __dot1, float2: __dot2, \
  float3: __dot3, float4: __dot4)(p0, p1)

#define length(p) native_sqrt(dot(p, p))
#define normalize(p) ((p)/length(p))

/* 6.11.6 Relational Functions */

#define select(a, b, c) ((c) ? (b) : (a))

/* 6.11.9 Explicit Memory Fence Functions */

typedef enum {
  CLK_LOCAL_MEM_FENCE = 1,
  CLK_GLOBAL_MEM_FENCE = 2
} cl_mem_fence_flags;

void mem_fence(cl_mem_fence_flags flags);
