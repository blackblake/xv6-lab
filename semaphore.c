#include "semaphore.h"

#include <errno.h>

int sem_init(semaphore_t *sem, const char *name, int initial_value) {
    if (sem == NULL || initial_value < 0) {
        return EINVAL;
    }

    int rc = base_lock_init(&sem->lock, name);
    if (rc != 0) {
        return rc;
    }

    rc = pthread_cond_init(&sem->cond, NULL);
    if (rc != 0) {
        base_lock_destroy(&sem->lock);
        return rc;
    }

    sem->value = initial_value;
    sem->name = name != NULL ? name : "(unnamed)";
    sem->wait_count = 0;
    sem->wait_block_count = 0;
    sem->signal_count = 0;
    sem->wake_count = 0;

    return 0;
}

int sem_wait(semaphore_t *sem) {
    if (sem == NULL) {
        return EINVAL;
    }

    int rc = base_lock_acquire(&sem->lock);
    if (rc != 0) {
        return rc;
    }

    sem->wait_count++;

    while (sem->value <= 0) {
        sem->wait_block_count++;
        rc = pthread_cond_wait(&sem->cond, base_lock_native_handle(&sem->lock));
        if (rc != 0) {
            base_lock_release(&sem->lock);
            return rc;
        }
        sem->wake_count++;
    }

    sem->value--;

    rc = base_lock_release(&sem->lock);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int sem_signal(semaphore_t *sem) {
    if (sem == NULL) {
        return EINVAL;
    }

    int rc = base_lock_acquire(&sem->lock);
    if (rc != 0) {
        return rc;
    }

    sem->signal_count++;
    sem->value++;

    if (sem->wait_block_count > sem->wake_count) {
        rc = pthread_cond_signal(&sem->cond);
        if (rc != 0) {
            base_lock_release(&sem->lock);
            return rc;
        }
    }

    rc = base_lock_release(&sem->lock);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int sem_get_value(semaphore_t *sem, int *out_value) {
    if (sem == NULL || out_value == NULL) {
        return EINVAL;
    }

    int rc = base_lock_acquire(&sem->lock);
    if (rc != 0) {
        return rc;
    }

    *out_value = sem->value;

    rc = base_lock_release(&sem->lock);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

void sem_print_status(semaphore_t *sem) {
    if (sem == NULL) {
        return;
    }

    if (base_lock_acquire(&sem->lock) != 0) {
        return;
    }

    const char *name = sem->name != NULL ? sem->name : "(unnamed)";

    printf("semaphore %s: value=%d, waits=%lu, blocked=%lu, wakeups=%lu, signals=%lu\n",
           name,
           sem->value,
           sem->wait_count,
           sem->wait_block_count,
           sem->wake_count,
           sem->signal_count);

    base_lock_release(&sem->lock);
}

int sem_destroy(semaphore_t *sem) {
    if (sem == NULL) {
        return EINVAL;
    }

    int rc = pthread_cond_destroy(&sem->cond);
    if (rc != 0) {
        return rc;
    }

    return base_lock_destroy(&sem->lock);
}
