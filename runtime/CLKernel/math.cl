#include <klee/clkernel.h>

#define DEFINE_NATIVE_DIVIDE(__gentype) \
__attribute__((overloadable)) __gentype native_divide(__gentype x, __gentype y) { \
  return x / y; \
}

DEFINE_NATIVE_DIVIDE(float)
DEFINE_NATIVE_DIVIDE(float2)
DEFINE_NATIVE_DIVIDE(float3)
DEFINE_NATIVE_DIVIDE(float4)
DEFINE_NATIVE_DIVIDE(float8)
DEFINE_NATIVE_DIVIDE(float16)
