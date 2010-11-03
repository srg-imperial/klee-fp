#define __local __attribute__((address_space(1)))
#define __constant __attribute__((address_space(2)))
#define __global __attribute__((address_space(3)))

#define __kernel

typedef __attribute__((ext_vector_type(2))) float float2;
typedef __attribute__((ext_vector_type(3))) float float3;
typedef __attribute__((ext_vector_type(4))) float float4;
typedef __attribute__((ext_vector_type(5))) float float5;
typedef __attribute__((ext_vector_type(6))) float float6;
typedef __attribute__((ext_vector_type(7))) float float7;
typedef __attribute__((ext_vector_type(8))) float float8;
typedef __attribute__((ext_vector_type(9))) float float9;
typedef __attribute__((ext_vector_type(10))) float float10;
typedef __attribute__((ext_vector_type(11))) float float11;
typedef __attribute__((ext_vector_type(12))) float float12;
typedef __attribute__((ext_vector_type(13))) float float13;
typedef __attribute__((ext_vector_type(14))) float float14;
typedef __attribute__((ext_vector_type(15))) float float15;
typedef __attribute__((ext_vector_type(16))) float float16;

typedef unsigned int uint;
typedef unsigned long ulong;

typedef ulong size_t;

uint get_work_dim(void);
size_t get_global_size(uint dimindx);
size_t get_global_id(uint dimindx);
size_t get_local_size(uint dimindx);
size_t get_local_id(uint dimindx);
size_t get_num_groups(uint dimindx);
size_t get_group_id(uint dimindx);
size_t get_global_offset(uint dimindx);
