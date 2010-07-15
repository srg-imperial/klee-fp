#include <klee/klee.h>
#include <stdio.h>

unsigned klee_sse_count = 0;

void klee_sse(char *name) {
	printf("SSE instr: %s\n", name);
	++klee_sse_count;
	//klee_stack_trace();
}
