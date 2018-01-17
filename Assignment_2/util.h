#ifndef UTIL_H
#define UTIL_H

#define _GNU_SOURCE
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define URCU_INLINE_SMALL_FUNCTIONS
#include <urcu/arch.h>
#include <urcu-qsbr.h>
#include <x86intrin.h>

#define printf_error(fmt, args...)		\
	do { \
		printf(fmt, args); fflush(stdout); \
	} while (0)

#define printf_verbose(fmt, args...)		\
	do { \
		if (PRINT_VERBOSE)	{	printf(fmt, args); fflush(stdout); } \
	} while (0)

#define printf_parent(fmt, args...)		\
	do { \
		if (PRINT_PARENT)	{	printf(fmt, args); fflush(stdout); } \
	} while (0)

#define printf_worker(fmt, args...)		\
	do { \
		if (PRINT_WORKER)	{	printf(fmt, args); fflush(stdout); } \
	} while (0)

#define printf_csv(fmt, args...)		\
	do { \
		if (PRINT_CSV)	{	printf(fmt, args); fflush(stdout); } \
	} while (0)

// If *ptr == old, atomically store new to *ptr and return old.
// Otherwise, return the value of *ptr without changing it.
#define lockcmpxchgq(ptr, old, new)         __N3WLN__ \
({									                        __N3WLN__ \
  uint64_t __ret;                           __N3WLN__ \
  uint64_t __old = (old);                   __N3WLN__ \
  uint64_t __new = (new);                   __N3WLN__ \
  volatile uint64_t *__ptr = (ptr);         __N3WLN__ \
  asm volatile("lock; cmpxchgq %2,%1"       __N3WLN__ \
              : "=a" (__ret), "+m" (*__ptr) __N3WLN__ \
              : "r" (__new), "0" (__old)    __N3WLN__ \
              : "memory");                  __N3WLN__ \
  __ret;                                    __N3WLN__ \
})

// Store new to *ptr, and return the immediately previous value in *ptr's
// coherence order.  Excerpt from a comment in glibc from
// nptl/pthread_spin_lock.c: 
// xchgq usually takes less instructions than
// lockcmpxchg.  On the other hand,
// lockcmpxchg potentially generates less bus traffic when the lock is locked.
#define xchgq(ptr, new)                      __N3WLN__ \
({                                           __N3WLN__ \
  uint64_t __new = (new);                    __N3WLN__ \
  volatile uint64_t *__ptr = (ptr);          __N3WLN__ \
  asm volatile ("xchgq %0, %1"               __N3WLN__ \
               : "=r" (__new), "=m" (*__ptr) __N3WLN__ \
               : "0" (__new), "m" (*__ptr)   __N3WLN__ \
               : "memory" );                 __N3WLN__ \
  __new;                                     __N3WLN__ \
})

// atomically add val to *ptr, returning the pre-add value of *ptr.
#define lockxaddq(ptr, val)                 __N3WLN__ \
({                                          __N3WLN__ \
  uint64_t __val = (val);                   __N3WLN__ \
  volatile uint64_t *__ptr = (ptr);         __N3WLN__ \
  asm volatile("lock; xaddq %0, %1"         __N3WLN__ \
              : "+r" (__val), "+m" (*__ptr) __N3WLN__ \
              : : "memory" );               __N3WLN__ \
  __val;                                    __N3WLN__ \
})

typedef struct {
  uint64_t operation;
//  uint64_t params[4];
} op;

#define CACHE_LINE                           8 // 8-byte words in a cache line
#define L1_DATA_CACHE                     4096 // 8-byte words in the 32KB L1 cache on Ivy Bridge
#define ACCESS_BUFFER_SIZE (2 * L1_DATA_CACHE)

typedef struct {
  int i;
  int n_ops_i;
  volatile uint64_t phase;
  volatile uint64_t my_spinlock_counter;
  struct drand48_data rand_state;
//  uint64_t buffer[ACCESS_BUFFER_SIZE];
  uint64_t *buffer;
  uint64_t buffer_cur;
} ti_data_in;

void set_affinity(void);

extern volatile uint64_t completed_phase;
extern uint64_t n_threads, n_cpus, n_ops, n_runs, n_tests, n_accesses;
extern op **oss;
extern char *test_names[];
extern uint64_t test_on[];
extern uint64_t accessesv[];
extern uint64_t global_buffer[];

// On Intel, the busy-wait-nop instruction is called "pause",
// which is actually represented as a nop with the rep prefix.
// On processors before the P4 this behaves as a nop; on P4 and
// later it might do something clever like yield to another
// hyperthread.  In any case, Intel recommends putting one
// of these in a spin lock loop.
#define spin_pause() do { __asm__ __volatile__ ("rep; nop"); } while (0)
#define nop() do { __asm__ __volatile__ ("nop"); } while (0)

#define access(i, d, n_words) do { \
  for((i)=0; (i)<(n_words); ++(i), (d)->buffer_cur = ((d)->buffer_cur + 1) % ACCESS_BUFFER_SIZE) { \
    _CMM_STORE_SHARED((d)->buffer[(d)->buffer_cur], 1 + (CMM_LOAD_SHARED((d)->buffer[(d)->buffer_cur]))); \
  } \
} while(0)

#if defined(PRINT_SPIN)
#define spin_until(p, phase, t_i) __N3WLN__ \
  do { __N3WLN__ \
  while(CMM_LOAD_SHARED(*p) != phase) { spin_pause(); } __N3WLN__ \
  printf_verbose("%d: finished spin waiting for phase %d\n", t_i, phase); __N3WLN__ \
  fflush(stdout); __N3WLN__ \
  } while (0) __N3WLN__ \

#else
#define spin_until(p, phase, t_i) __N3WLN__ \
  do { __N3WLN__ \
  while(CMM_LOAD_SHARED(*p) != phase) { spin_pause(); } __N3WLN__ \
  } while (0) __N3WLN__ \

#endif

#if defined(PRINT_SPIN)
#define sleep_until(p, phase, t_i) __N3WLN__ \
  do { __N3WLN__ \
  while(CMM_LOAD_SHARED(*p) != phase) { usleep(100); spin_pause(); } __N3WLN__ \
  printf_verbose("%d: finished spin waiting for phase %d\n", t_i, phase); __N3WLN__ \
  fflush(stdout); __N3WLN__ \
  } while (0) __N3WLN__ \

#else
#define sleep_until(p, phase, t_i) __N3WLN__ \
  do { __N3WLN__ \
  while(CMM_LOAD_SHARED(*p) != phase) { usleep(100); spin_pause(); } __N3WLN__ \
  } while (0) __N3WLN__ \

#endif

#endif
