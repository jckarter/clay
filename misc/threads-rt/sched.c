#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "proc.h"

static int max_threads;
static scheduler_thread_t *threads;
static pthread_key_t key_self;

void task_add_to(scheduler_thread_t *p, task_t* task) {
    //printf("A: %u %u\n", task, task->affinity);
    pthread_mutex_lock(&p->lock);
    if (p->waiting) {
        p->waiting_tail->next = task;
        p->waiting_tail = task;
    } else {
        p->waiting = p->waiting_tail = task;
    }
    pthread_cond_signal(&p->cond);
    pthread_mutex_unlock(&p->lock);
}

void task_add(task_t *task) {
    //int thread = random() % max_threads;
    //task->affinity = &threads[thread];
    task_add_to(task->affinity, task);
}

task_t *task_remove(scheduler_thread_t *p) {
    pthread_mutex_lock(&p->lock);
    if (!p->waiting) {
        pthread_cond_wait(&p->cond, &p->lock);
    }
    task_t *task = p->waiting;
    p->waiting = p->waiting->next;
    pthread_mutex_unlock(&p->lock);
    task->next = NULL;
    return task;
}

void sched_set_max_threads(int val) {
    max_threads = val;
}

void sched_init() {
    int i;

    max_threads = max_threads > 1 ? max_threads : 1;
    threads = malloc(sizeof(scheduler_thread_t) * max_threads);

    pthread_key_create(&key_self, NULL);

    for (i = 0; i < max_threads; ++i) {
        threads[i].current = NULL;
        threads[i].waiting = NULL;
        threads[i].task_count = 0;
        pthread_mutex_init(&threads[i].lock, NULL);
        pthread_cond_init(&threads[i].cond, NULL);
    }
}

scheduler_thread_t *sched_self() {
    return pthread_getspecific(key_self);
}

void sched_enter() {
    scheduler_thread_t *p = sched_self();
    swap_cpu_context(&p->current->context, &p->context);
}

void sched_destroy() {
    // FIXME
}

task_t *sched_task_create(size_t stacksize, void* (*func)(void *, long), 
        void *ptr, long val) {
    int i;
    task_t *task = malloc(sizeof(task_t) + 8191);
    task->next = NULL;
    task->chnext = NULL;
    task->affinity = NULL;
    get_cpu_context(&task->context);

    // We must set the initial affinity
    scheduler_thread_t *best = &(threads[0]);
    int best_count = best->task_count;
    for (i = 1; i < max_threads; ++i) {
        if (threads[i].task_count < best_count) {
            best_count = threads[i].task_count;
            best = &(threads[i]);
        }
    }
    best->task_count ++;
    task->affinity = best;

    make_cpu_context(&task->context, task->stack, 8192,  
            (void (*)())func, 2, ptr, val);
    return task;
}

// I think this can be optimized
void sched_self_ready() {
    scheduler_thread_t *p = sched_self();
    assert(p->current->affinity == p);
    task_add_to(p, p->current); // FIXME ? Does this always hold true
    sched_enter();
}

void sched_self_suspend() {
    //printf("S: %u\n", sched_self()->current);
    sched_enter();
}

void sched_ready(task_t *task) {
    task_add(task);
}

void sched_self_exit() {
    scheduler_thread_t *p = sched_self();
    free(p->current);
    //printf("Exiting\n");
    p->current = NULL;
    p->task_count--;
    set_cpu_context(&p->context);
}

void *scheduler(void *p_) {
    scheduler_thread_t *p = p_;
    get_cpu_context(&p->context);

    pthread_setspecific(key_self, p);

    while (1) {
        task_t *t = task_remove(p);
        //printf("R: %u\n", t);
        p->current = t;
        swap_cpu_context(&p->context, &t->context);
    }

    return NULL;
}

void sched_run() {
    int i; 
    
    for (i = 0; i < max_threads; ++i) {
        pthread_create(&(threads[i].thread), NULL, scheduler, &threads[i]);
    }

    // All the threads are now running. Wait for all to terminate
    for (i = 0; i < max_threads; ++i) {
        pthread_join(threads[i].thread, NULL);
    }

    free(threads);
}
