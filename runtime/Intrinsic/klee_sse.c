#include <klee/klee.h>
#include <stdio.h>

unsigned klee_sse_count = 0;

void klee_sse(char *name, char *file, unsigned line, char* func) {
	printf("SSE instr: %s at %s:%d, in %s\n", name, file, line, func);
	++klee_sse_count;
	//klee_stack_trace();
}
