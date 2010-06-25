#include <errno.h>
#include <math.h>

double llvm_sqrt(double) asm("llvm.sqrt.f64");

double sqrt(double d) {
#if 0
  if (d < 0.0)
    errno = EDOM;
#endif
  return llvm_sqrt(d);
}

float llvm_sqrtf(float) asm("llvm.sqrt.f32");

float sqrtf(float f) {
#if 0
  if (f < 0.0f)
    errno = EDOM;
#endif
  return llvm_sqrtf(f);
}
