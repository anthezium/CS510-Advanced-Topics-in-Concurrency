#ifndef WORKER_H
#define WORKER_H

#include "util.h"

void* ti(void *data);

int spin_try_lock(volatile uint64_t *lock) __attribute__((always_inline));
void spin_lock(volatile uint64_t *lock) __attribute__((always_inline));
void spin_wait_lock(volatile uint64_t *lock) __attribute__((always_inline));
void spin_read_lock(volatile uint64_t *lock) __attribute__((always_inline));
void spin_unlock(volatile uint64_t *lock) __attribute__((always_inline));
void spin_experimental_lock(volatile uint64_t *lock) __attribute__((always_inline));
#endif
