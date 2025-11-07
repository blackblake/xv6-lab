#include "base_lock.h"

#include <errno.h>

int base_lock_init(base_lock_t *lock, const char *name) {
    if (lock == NULL) {
        return EINVAL;
    }

    lock->name = name;
    return pthread_mutex_init(&lock->mutex, NULL);
}

int base_lock_destroy(base_lock_t *lock) {
    if (lock == NULL) {
        return EINVAL;
    }

    return pthread_mutex_destroy(&lock->mutex);
}

int base_lock_acquire(base_lock_t *lock) {
    if (lock == NULL) {
        return EINVAL;
    }

    return pthread_mutex_lock(&lock->mutex);
}

int base_lock_release(base_lock_t *lock) {
    if (lock == NULL) {
        return EINVAL;
    }

    return pthread_mutex_unlock(&lock->mutex);
}

pthread_mutex_t *base_lock_native_handle(base_lock_t *lock) {
    if (lock == NULL) {
        return NULL;
    }

    return &lock->mutex;
}
