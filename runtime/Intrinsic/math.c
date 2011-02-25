#include <errno.h>
#include <math.h>
#include <klee/klee.h>

double sqrt(double d) {
#if 0
  if (d < 0.0)
    errno = EDOM;
#endif
  return klee_sqrt(d);
}

float sqrtf(float f) {
#if 0
  if (f < 0.0f)
    errno = EDOM;
#endif
  return klee_sqrtf(f);
}

double cos(double d) {
  return klee_cos(d);
}

float cosf(float f) {
  return klee_cosf(f);
}

double sin(double d) {
  return klee_sin(d);
}

float sinf(float f) {
  return klee_sinf(f);
}
