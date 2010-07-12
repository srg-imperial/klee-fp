#include <klee/klee.h>

unsigned klee_sse_count = 0;

void klee_sse(void) {
	++klee_sse_count;
}
