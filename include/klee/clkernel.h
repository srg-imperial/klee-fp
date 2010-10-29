#define __local __attribute__((address_space(1)))
#define __constant __attribute__((address_space(2)))
#define __global __attribute__((address_space(3)))
