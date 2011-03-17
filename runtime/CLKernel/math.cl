#include <klee/clkernel.h>
#include <klee/klee.h>

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

float native_cos(float x) {
  return klee_cosf(x);
}

float native_sin(float x) {
  return klee_sinf(x);
}

float native_sqrt(float x) {
  return klee_sqrtf(x);
}
