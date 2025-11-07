#ifndef BASE_LOCK_H
#define BASE_LOCK_H

#include <pthread.h>

typedef struct base_lock {
    pthread_mutex_t mutex;
    const char *name;
} base_lock_t;

int base_lock_init(base_lock_t *lock, const char *name);
int base_lock_destroy(base_lock_t *lock);
int base_lock_acquire(base_lock_t *lock);
int base_lock_release(base_lock_t *lock);
pthread_mutex_t *base_lock_native_handle(base_lock_t *lock);

#endif // BASE_LOCK_H
