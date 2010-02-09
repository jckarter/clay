#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "proc.h"

void waiter_add(task_t **list, void *ptr, task_t *task) {
    task_t *p = *list;
    task->ptr = ptr;
    if (!p) {
        *list = task;
    } else {
        while(p->chnext) {
            p = p->chnext;
        }
        p->chnext = task;
    }
}

task_t *waiter_remove(task_t **list) {
    task_t *task = *list;

    assert(task); /* function called only when the list has >1 elements */
    *list = (*list)->chnext;
    task->chnext = NULL;

    return task;
}

channel_t *chan_create(size_t size) {
    channel_t *c = malloc(sizeof(channel_t));
    memset(c, 0, sizeof(channel_t));
    pthread_mutex_init(&c->lock, NULL);
    c->size = size;
    return c; 
}

// Used when channel is on a stack
void chan_init(channel_t *c, size_t size) {
    memset(c, 0, sizeof(channel_t));
    c->size = size;
    pthread_mutex_init(&c->lock, NULL);
}

void chan_destroy(channel_t *c) {
    free(c);
}

void chan_send(channel_t *c, void *ptr) {
    pthread_mutex_lock(&c->lock);
    if (c->receivers) {
        memcpy(c->receivers->ptr, ptr, c->size);
        task_t *waiter = waiter_remove(&c->receivers);
        pthread_mutex_unlock(&c->lock);
        sched_ready(waiter);
        sched_self_ready();
    } else {
        waiter_add(&c->senders, ptr, sched_self()->current);
        pthread_mutex_unlock(&c->lock);
        sched_self_suspend();
    }
}

void chan_recv(channel_t *c, void *ptr) {
    pthread_mutex_lock(&c->lock);
    if (c->senders) {
        memcpy(ptr, c->senders->ptr, c->size);
        task_t *waiter = waiter_remove(&c->senders);
        pthread_mutex_unlock(&c->lock);
        sched_ready(waiter);
        sched_self_ready();
    } else {
        waiter_add(&c->receivers, ptr, sched_self()->current);
        pthread_mutex_unlock(&c->lock);
        sched_self_suspend();
    }
}
