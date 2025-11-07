#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <pthread.h>
#include <stdio.h>

#include "base_lock.h"

typedef struct semaphore {
    int value;
    base_lock_t lock;
    pthread_cond_t cond;
    const char *name;
    unsigned long wait_count;
    unsigned long wait_block_count;
    unsigned long signal_count;
    unsigned long wake_count;
} semaphore_t;

int sem_init(semaphore_t *sem, const char *name, int initial_value);
int sem_wait(semaphore_t *sem);
int sem_signal(semaphore_t *sem);
int sem_get_value(semaphore_t *sem, int *out_value);
void sem_print_status(semaphore_t *sem);
int sem_destroy(semaphore_t *sem);

#endif // SEMAPHORE_H
