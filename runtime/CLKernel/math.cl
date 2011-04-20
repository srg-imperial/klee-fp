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

float __dot1(float p0, float p1) {
  return p0*p1;
}

float __dot2(float2 p0, float2 p1) {
  return p0.x*p1.x + p0.y*p1.y;
}

float __dot3(float3 p0, float3 p1) {
  return p0.x*p1.x + p0.y*p1.y + p0.z*p1.z;
}

float __dot4(float4 p0, float4 p1) {
  return p0.x*p1.x + p0.y*p1.y + p0.z*p1.z + p0.w*p1.w;
}
