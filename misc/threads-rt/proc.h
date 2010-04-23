#ifndef _PROC_H_
#define _PROC_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__i386__)

struct __context {
	/*
	 * The first 20 fields must match the definition of
	 * sigcontext. So that we can support sigcontext
	 * and ucontext_t at the same time.
	 */
	int	mc_onstack;		/* XXX - sigcontext compat. */
	int	mc_gs;
	int	mc_fs;
	int	mc_es;
	int	mc_ds;
	int	mc_edi;
	int	mc_esi;
	int	mc_ebp;
	int	mc_isp;
	int	mc_ebx;
	int	mc_edx;
	int	mc_ecx;
	int	mc_eax;
	int	mc_trapno;
	int	mc_err;
	int	mc_eip;
	int	mc_cs;
	int	mc_eflags;
	int	mc_esp;			/* machine state */
	int	mc_ss;

	int	mc_fpregs[28];		/* env87 + fpacc87 + u_long */
	int	__spare__[17];
};

#elif defined(__x86_64__)

struct __context {
	/*
	 * The first 20 fields must match the definition of
	 * sigcontext. So that we can support sigcontext
	 * and ucontext_t at the same time.
	 */
	long	mc_onstack;		/* XXX - sigcontext compat. */
	long	mc_rdi;			/* machine state (struct trapframe) */
	long	mc_rsi;
	long	mc_rdx;
	long	mc_rcx;
	long	mc_r8;
	long	mc_r9;
	long	mc_rax;
	long	mc_rbx;
	long	mc_rbp;
	long	mc_r10;
	long	mc_r11;
	long	mc_r12;
	long	mc_r13;
	long	mc_r14;
	long	mc_r15;
	long	mc_trapno;
	long	mc_addr;
	long	mc_flags;
	long	mc_err;
	long	mc_rip;
	long	mc_cs;
	long	mc_rflags;
	long	mc_rsp;
	long	mc_ss;

	long	mc_len;			/* sizeof(mcontext_t) */
#define	_MC_FPFMT_NODEV		0x10000	/* device not present or configured */
#define	_MC_FPFMT_XMM		0x10002
	long	mc_fpformat;
#define	_MC_FPOWNED_NONE	0x20000	/* FP state not used */
#define	_MC_FPOWNED_FPU		0x20001	/* FP state came from FPU */
#define	_MC_FPOWNED_PCB		0x20002	/* FP state came from PCB */
	long	mc_ownedfp;
	/*
	 * See <machine/fpu.h> for the internals of mc_fpstate[].
	 */
	long	mc_fpstate[64];
	long	mc_spare[8];
};

#endif

typedef struct __context cpu_context_t;

struct task_t {
    struct task_t *next;
    struct task_t *chnext; /* For channel waiter */
    void *ptr; /* Channel data */
    void *affinity; /* Scheduler it belongs to */
    cpu_context_t context;
    char stack[1];
};

typedef struct task_t task_t;

struct scheduler_thread_t {
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    cpu_context_t context;
    task_t *current;
    task_t *waiting;
    task_t *waiting_tail;
    int task_count;
};

typedef struct scheduler_thread_t scheduler_thread_t;

int swap_cpu_context(cpu_context_t*, const cpu_context_t *);
int get_cpu_context(cpu_context_t*);
void make_cpu_context(cpu_context_t*, void *, int, void (*)(), int, ...);
void set_cpu_context(const cpu_context_t*);

/*
struct chan_waiter_t{
    task_t *task;
    void *ptr;
    struct chan_waiter_t *next;
};

typedef struct chan_waiter_t chan_waiter_t;
*/

struct channel_t {
    size_t size;
    /* TODO: keep tail pointers to make inserts O(1) */
    task_t *senders;
    task_t *receivers; 
    pthread_mutex_t lock;
};

typedef struct channel_t channel_t;

void sched_init();
void sched_destroy();
void sched_run();
void sched_self_ready();
void sched_self_exit();
void sched_set_max_threads(int);
scheduler_thread_t *sched_self();
void sched_ready(task_t *task);
void sched_self_suspend();
task_t *sched_task_create(size_t stacksize, void* (*)(void *, long), 
        void *, long); 

channel_t *chan_create(size_t size);
void chan_init(channel_t *c, size_t size);
void chan_destroy(channel_t *c);
void chan_send(channel_t *c, void *ptr);
void chan_recv(channel_t *c, void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* _PROC_H_ */
