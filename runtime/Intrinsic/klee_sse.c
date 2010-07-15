#include <klee/klee.h>
#include <stdio.h>

unsigned klee_sse_count = 0;

void klee_sse(char *name, char *file, unsigned line) {
	printf("SSE instr: %s at %s:%d\n", name, file, line);
	++klee_sse_count;
	//klee_stack_trace();
}
