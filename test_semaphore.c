#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "semaphore.h"

// Build: gcc -o test_sem base_lock.c semaphore.c test_semaphore.c -lpthread
// Run: ./test_sem

#define THREAD_COUNT 8

static struct semaphore test_sem;
static int shared_counter = 0;

struct thread_arg {
    int id;
};

static void *worker(void *arg) {
    struct thread_arg *targ = (struct thread_arg *)arg;

    printf("[thread %d] waiting for semaphore\n", targ->id);
    sem_acquire(&test_sem);
    printf("[thread %d] acquired semaphore\n", targ->id);

    int before = shared_counter;
    shared_counter = before + 1;
    printf("[thread %d] counter incremented from %d to %d\n", targ->id, before, shared_counter);

    printf("[thread %d] releasing semaphore\n", targ->id);
    sem_release(&test_sem);
    return NULL;
}

int main(void) {
    pthread_t threads[THREAD_COUNT];
    struct thread_arg args[THREAD_COUNT];

    sem_init(&test_sem, 1);

    for (int i = 0; i < THREAD_COUNT; i++) {
        args[i].id = i;
        if (pthread_create(&threads[i], NULL, worker, &args[i]) != 0) {
            perror("pthread_create");
            sem_destroy(&test_sem);
            return 1;
        }
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");
            sem_destroy(&test_sem);
            return 1;
        }
    }

    printf("Final counter value: %d\n", shared_counter);
    sem_print_status(&test_sem);
    sem_destroy(&test_sem);

    return 0;
}
