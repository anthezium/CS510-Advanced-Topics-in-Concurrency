#ifndef TESTS_H
#define TESTS_H

#include <sys/time.h>

#include "util.h"

// GLOBAL DATA

extern pthread_spinlock_t pthreads_spinlock;
extern volatile uint64_t  my_spinlock;
extern volatile uint64_t  my_spinlock_shared_counter;

#define UNLOCKED 0
#define LOCKED   1

// OPERATIONS
// must all be unique

#define PTHREAD_SPIN_LOCK_LOCK_OP       1
#define SPIN_LOCK_LOCK_OP               2
#define SPIN_LOCK_INC_OP                3
#define SPIN_WAIT_LOCK_LOCK_OP          4
#define SPIN_WAIT_LOCK_INC_OP           5
#define SPIN_READ_LOCK_LOCK_OP          6
#define SPIN_READ_LOCK_INC_OP           7
#define SPIN_TRY_LOCK_LOCK_OP           8
#define SPIN_TRY_LOCK_INC_OP            9
#define SPIN_EXPERIMENTAL_LOCK_LOCK_OP 10
#define SPIN_EXPERIMENTAL_LOCK_INC_OP  11

// PHASES

#define INIT_PHASE   0
#define READY_PHASE  1
#define WARMED_PHASE 2
#define OPS_PHASE    3

// TESTS
// must be the sequence of natural numbers from 0 to the
// total number of tests - 1.

#define SPIN_TRY_LOCK_CORRECTNESS_TEST          0
#define SPIN_LOCK_CORRECTNESS_TEST              1
#define SPIN_WAIT_LOCK_CORRECTNESS_TEST         2
#define SPIN_READ_LOCK_CORRECTNESS_TEST         3
#define SPIN_EXPERIMENTAL_LOCK_CORRECTNESS_TEST 4
#define PTHREAD_SPIN_LOCK_TEST                  5
#define SPIN_TRY_LOCK_TEST                      6
#define SPIN_LOCK_TEST                          7
#define SPIN_WAIT_LOCK_TEST                     8
#define SPIN_READ_LOCK_TEST                     9
#define SPIN_EXPERIMENTAL_LOCK_TEST            10

#define PASTE31(x, y, z) x ## y ## z
#define PASTE3(x, y, z) PASTE31(x, y, z)

#define PASTE21(x, y) x ## y
#define PASTE2(x, y) PASTE21(x, y)

#define STR_VALUE(arg)      #arg
#define FUNCTION_NAME(name) STR_VALUE(name)

#define TIME_AND_CHECK(uid, t_i, run, fcall, label, dst, ret) __N3WLN__ \
  do { __N3WLN__ \
    uint64_t (PASTE3(pre,__LINE__,uid)), (PASTE3(post,__LINE__,uid)); __N3WLN__ \
    struct timeval  (PASTE3(tv1,__LINE__,uid)), (PASTE3(tv2,__LINE__,uid)); __N3WLN__ \
    double (PASTE3(secs,__LINE__,uid)); __N3WLN__ \
    unsigned int (PASTE3(ui,__LINE__,uid)); __N3WLN__ \
    printf_parent("%d: run %d: about to call %s [%s]:\n", t_i, run, FUNCTION_NAME(fcall), label); __N3WLN__ \
    gettimeofday(&(PASTE3(tv1,__LINE__,uid)), NULL); __N3WLN__ \
    (PASTE3(pre,__LINE__,uid)) = __rdtscp(&(PASTE3(ui,__LINE__,uid))); __N3WLN__ \
    cmm_barrier(); __N3WLN__ \
    ret = fcall; __N3WLN__ \
    cmm_barrier(); __N3WLN__ \
    (PASTE3(post,__LINE__,uid)) = __rdtscp(&(PASTE3(ui,__LINE__,uid))); __N3WLN__ \
    gettimeofday(&(PASTE3(tv2,__LINE__,uid)), NULL); __N3WLN__ \
    dst = (PASTE3(post,__LINE__,uid)) - (PASTE3(pre,__LINE__,uid)); __N3WLN__ \
    (PASTE3(secs,__LINE__,uid)) = (double) ((PASTE3(tv2,__LINE__,uid)).tv_usec - (PASTE3(tv1,__LINE__,uid)).tv_usec) / 1000000 + (double) ((PASTE3(tv2,__LINE__,uid)).tv_sec - (PASTE3(tv1,__LINE__,uid)).tv_sec); __N3WLN__ \
    printf_parent("%d: run %d: call to %s [%s] took %" PRIu64 " TSC ticks, %f seconds, returned %d\n", t_i, run, FUNCTION_NAME(fcall), label, dst, (PASTE3(secs,__LINE__,uid)), ret); __N3WLN__ \
    if(!ret) { __N3WLN__ \
      printf_parent("%d: run %d: call succeeded (returned 0)\n", t_i, run); __N3WLN__ \
    } __N3WLN__ \
    else { __N3WLN__ \
      printf_parent("%d: run %d: call failed (returned %d)\n", t_i, run, ret); __N3WLN__ \
    } __N3WLN__ \
  } while (0) __N3WLN__ \

#endif
