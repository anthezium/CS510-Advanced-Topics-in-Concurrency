#ifndef TESTS_H
#define TESTS_H

#include <sys/time.h>

#include "util.h"

// GLOBAL DATA

extern pthread_spinlock_t pthreads_spinlock;
extern volatile uint64_t  my_spinlock;
extern volatile uint64_t  my_spinlock_shared_counter;

// define a type for ticket
typedef struct {
  uint64_t next;
  uint64_t padding[7];
  uint64_t owner;
} ticket_state;

// declare shared data for ticket

extern volatile ticket_state my_ticket_lock __attribute__((aligned (64)));

// define a type for abql_sharing
typedef struct {
  uint64_t val;
} flag_sharing;

// declare shared data for abql_sharing

extern volatile uint64_t      queue_last_sharing;
extern volatile flag_sharing *flags_sharing;

// TODO define a type for abql_nosharing (alter the definition below)
typedef struct {
  uint64_t val;
} flag_nosharing;

// declare shared data for abql_nosharing

extern volatile uint64_t        queue_last_nosharing;
extern volatile flag_nosharing *flags_nosharing;

// define a type for mcs_sharing
typedef struct {
  uint64_t next;
  uint64_t locked;
} mcs_sharing;

// declare shared data for mcs_sharing

extern volatile mcs_sharing mcs_global_sharing;
extern volatile mcs_sharing *mcss_sharing;

// TODO define a type for mcs_nosharing (alter the definition below)
typedef struct {
  uint64_t next;
  uint64_t locked;
} mcs_nosharing;

// declare shared data for mcs_nosharing

extern volatile mcs_nosharing mcs_global_nosharing __attribute__((aligned (64)));
extern volatile mcs_nosharing *mcss_nosharing;

// LOCK VALUES

#define UNLOCKED  0
#define LOCKED    1

#define HAS_LOCK  2
#define MUST_WAIT 3

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
#define ABQL_SHARING_LOCK_OP           12
#define ABQL_SHARING_INC_OP            13
#define ABQL_NOSHARING_LOCK_OP         14
#define ABQL_NOSHARING_INC_OP          15
#define MCS_SHARING_LOCK_OP            16
#define MCS_SHARING_INC_OP             17
#define MCS_NOSHARING_LOCK_OP          18
#define MCS_NOSHARING_INC_OP           19
#define TICKET_LOCK_OP                 20
#define TICKET_INC_OP                  21

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
#define TICKET_CORRECTNESS_TEST                 5
#define ABQL_SHARING_CORRECTNESS_TEST           6
#define ABQL_NOSHARING_CORRECTNESS_TEST         7
#define MCS_SHARING_CORRECTNESS_TEST            8
#define MCS_NOSHARING_CORRECTNESS_TEST          9
#define PTHREAD_SPIN_LOCK_TEST                 10
#define SPIN_TRY_LOCK_TEST                     11
#define SPIN_LOCK_TEST                         12
#define SPIN_WAIT_LOCK_TEST                    13
#define SPIN_READ_LOCK_TEST                    14
#define SPIN_EXPERIMENTAL_LOCK_TEST            15
#define TICKET_TEST                            16
#define ABQL_SHARING_TEST                      17
#define ABQL_NOSHARING_TEST                    18
#define MCS_SHARING_TEST                       19
#define MCS_NOSHARING_TEST                     20

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
