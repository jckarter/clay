#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "proc.h"

#if defined(__i386__)

void make_cpu_context(cpu_context_t *cp, void *ss_sp, int ss_size, 
        void (*func)(), int argc, ...)
{
	int *sp;

	sp = (int*)ss_sp + ss_size/sizeof(long);
	sp -= argc;
	sp = (void*)((uintptr_t)sp - (uintptr_t)sp%16);	/* 16-align for OS X */
	memmove(sp, &argc+1, argc*sizeof(int));

	*--sp = 0;		/* return address */
	cp->mc_eip = (long)func;
	cp->mc_esp = (int)sp;
}

#elif defined(__x86_64__) 

void make_cpu_context(cpu_context_t *cp, void *ss_sp, int ss_size, 
        void (*func)(), int argc, ...)
{
	long *sp;
	va_list va;

	memset(cp, 0, sizeof(cpu_context_t));
	if(argc != 2)
		*(int*)0 = 0;
	va_start(va, argc);
	cp->mc_rdi = va_arg(va, long);
	cp->mc_rsi = va_arg(va, int);
	va_end(va);
	sp = (long*)ss_sp + ss_size/sizeof(long);
	sp -= argc;
	sp = (void*)((uintptr_t)sp - (uintptr_t)sp%16);	/* 16-align for OS X */
	*--sp = 0;	/* return address */
	cp->mc_rip = (long)func;
	cp->mc_rsp = (long)sp;
}
#endif

int
swap_cpu_context(cpu_context_t *ocp, const cpu_context_t *cp)
{
	if(get_cpu_context(ocp) == 0)
		set_cpu_context(cp);
	return 0;
}

