#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "syscall.h"
#include "lib.h"

#define THREAD_NUM 6
#define THREAD_NAME_SIZE 15

// thread context
typedef struct _kz_context {
	uint32 sp; // stack pointer
} kz_context;

// task control block(TCB)
typedef struct _kz_thread {
	struct _kz_thread *next;
	char name[THREAD_NAME_SIZE + 1]; // thread name
	char *stack; // stack

	struct {
		kz_func_t func;
		int argc;
		char **argv;
	} init;

	struct {
		kz_syscall_type_t type;
		kz_syscall_param_t *param;
	} syscall;

	kz_context context;
} kz_thread;

static struct {
	kz_thread *head;
	kz_thread *tail;
} readyqueue;

static kz_thread *current; // current thread
static kz_thread threads[THREAD_NUM]; // task control block
static kz_handler_t handlers[SOFTVEC_TYPE_NUM]; // interrupt handlers

void dispatch(kz_context *context);

static int getcurrent(void) {
	if (current == NULL) {
		return -1;
	}

	readyqueue.head = current->next;
	if (readyqueue.head == NULL) {
		readyqueue.tail = NULL;
	}
	current->next = NULL;

	return 0;
}

static int putcurrent(void) {
	if (current == NULL) {
		return -1;
	}

	if (readyqueue.tail) {
		readyqueue.tail->next = current;
	} else {
		readyqueue.head = current;
	}
	readyqueue.tail = current;

	return 0;
}

static void thread_end(void) {
	kz_exit();
}

static void thread_init(kz_thread *thp) {
	thp->init.func(thp->init.argc, thp->init.argv);
	thread_end();
}

static kz_thread_id_t thread_run(kz_func_t func, char *name, int stacksize, int argc, char *argv[]) {
	int i;
	kz_thread *thp;
	uint32 *sp;
	extern char userstack;
	static char *thread_stack = &userstack;

	for (i = 0; i < THREAD_NUM; i++) {
		thp = &threads[i];
		if (thp->init.func == NULL) {
			break;
		}
	}

	if (i == THREAD_NUM) {
		return -1;
	}

	memset(thp, 0, sizeof(*thp));

	strcpy(thp->name, name);
	thp->next = NULL;
	thp->init.func = func;
	thp->init.argc = argc;
	thp->init.argv = argv;

	memset(thread_stack, 0, stacksize);
	thread_stack += stacksize;
	thp->stack = thread_stack;

	sp = (uint32 *)thp->stack;
	*(--sp) = (uint32)thread_end;

	*(--sp) = (uint32)thread_init;
	*(--sp) = 0; // ER6
	*(--sp) = 0; // ER5
	*(--sp) = 0; // ER4
	*(--sp) = 0; // ER3
	*(--sp) = 0; // ER2
	*(--sp) = 0; // ER1

	*(--sp) = (uint32)thp; // ER0

	thp->context.sp = (uint32)sp;

	putcurrent();

	current = thp;
	putcurrent();

	return (kz_thread_id_t)current;
}

static int thread_exit(void) {
	puts(current->name);
	puts(" EXIT.\n");
	memset(current, 0, sizeof(*current));
	return 0;
}

static int setintr(softvec_type_t type, kz_handler_t handler) {
	static void thread_intr(softvec_type_t type, unsigned long sp);

	softvec_setintr(type, thread_intr);
	handlers[type] = handler;

	return 0;
}

static void call_functions(kz_syscall_type_t type, kz_syscall_param_t *param) {
	switch(type) {
		case KZ_SYSCALL_TYPE_RUN: // kz_run()
			param->un.run.ret = thread_run(param->un.run.func, param->un.run.name,
					param->un.run.stacksize, param->un.run.argc, param->un.run.argv);
			break;
		case KZ_SYSCALL_TYPE_EXIT: // kz_exit()
			thread_exit();
			break;
		default:
			break;
	}
}

static void syscall_proc(kz_syscall_type_t type, kz_syscall_param_t *param) {
	getcurrent();
	call_functions(type, param);
}

static void schedule(void) {
	if (!readyqueue.head) {
		kz_sysdown();
	}
	current = readyqueue.head;
}

static void syscall_intr(void) {
	syscall_proc(current->syscall.type, current->syscall.param);
}

static void softerr_intr(void) {
	puts(current->name);
	puts(" DOWN.\n");
	getcurrent();
	thread_exit();
}

static void thread_intr(softvec_type_t type, unsigned long sp) {
	current->context.sp = sp;

	if (handlers[type]) {
		handlers[type]();
	}

	schedule();
	dispatch(&current->context);
}

void kz_start(kz_func_t func, char *name, int stacksize, int argc, char *argv[]) {
	current = NULL;

	readyqueue.head = readyqueue.tail = NULL;
	memset(threads, 0, sizeof(threads));
	memset(handlers, 0, sizeof(handlers));

	setintr(SOFTVEC_TYPE_SYSCALL, syscall_intr);
	setintr(SOFTVEC_TYPE_SOFTERR, softerr_intr);

	current = (kz_thread *)thread_run(func, name, stacksize, argc, argv);

	dispatch(&current->context);
}

void kz_sysdown(void) {
	puts("system error.\n");
	while(1) {
		;
	}
}

void kz_syscall(kz_syscall_type_t type, kz_syscall_param_t *param) {
	current->syscall.type = type;
	current->syscall.param = param;
	asm volatile ("trapa #0");
}
