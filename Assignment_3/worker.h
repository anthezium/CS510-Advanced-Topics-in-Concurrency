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
void abql_sharing_lock(volatile uint64_t *my_place, volatile uint64_t *queue_last, 
                       volatile flag_sharing *flags, uint64_t n_threads) __attribute__((always_inline));
void abql_sharing_unlock(volatile uint64_t *my_place, volatile flag_sharing *flags, 
                         uint64_t n_threads) __attribute__((always_inline));
void abql_nosharing_lock(volatile uint64_t *my_place, volatile uint64_t *queue_last, 
                         volatile flag_nosharing *flags, uint64_t n_threads) __attribute__((always_inline));
void abql_nosharing_unlock(volatile uint64_t *my_place, volatile flag_nosharing *flags, 
                           uint64_t n_threads) __attribute__((always_inline));
void mcs_sharing_lock(volatile mcs_sharing *global_lock, volatile mcs_sharing *local_lock) __attribute__((always_inline));
void mcs_nosharing_lock(volatile mcs_nosharing *global_lock, volatile mcs_nosharing *local_lock) __attribute__((always_inline));
void mcs_sharing_unlock(volatile mcs_sharing *global_lock, volatile mcs_sharing *local_lock) __attribute__((always_inline));
void mcs_nosharing_unlock(volatile mcs_nosharing *global_lock, volatile mcs_nosharing *local_lock) __attribute__((always_inline));
void ticket_lock(volatile ticket_state *lock) __attribute__((always_inline));
void ticket_unlock(volatile ticket_state *lock) __attribute__((always_inline));
void coarse_enqueue(ti_data_in *d, volatile coarse_queue *queue, uint64_t value) __attribute__((always_inline));
int coarse_dequeue(ti_data_in *d, volatile coarse_queue *queue, uint64_t *ret) __attribute__((always_inline));
void nb_enqueue(ti_data_in *d, volatile nb_queue *queue, uint64_t value) __attribute__((always_inline));
int nb_dequeue(ti_data_in *d, volatile nb_queue *queue, uint64_t *ret) __attribute__((always_inline));
#endif
