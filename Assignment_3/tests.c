#include <pthread.h>

#include "tests.h"
#include "util.h"
#include "worker.h"

void parent_announce(uint64_t phase, int t_i) __attribute__((always_inline));
void parent_spin(ti_data_in *datas, uint64_t phase, int t_i) __attribute__((always_inline));
int parent_announce_then_spin(ti_data_in *datas, uint64_t phase1, uint64_t phase2, int t_i) __attribute__((always_inline));

// GLOBAL DATA (statically determined initial values)

pthread_spinlock_t       pthreads_spinlock;
volatile uint64_t        my_spinlock                = UNLOCKED;
volatile uint64_t        my_spinlock_shared_counter = 0;
volatile uint64_t        completed_phase            = INIT_PHASE;
uint64_t                 global_buffer[ACCESS_BUFFER_SIZE];
uint64_t                *global_dequeues            = NULL;
ti_data_in              *global_datas               = NULL;
pool_meta               *global_pool_meta           = NULL;
uint64_t                *global_pool_data           = NULL;

// statically initialize global data for ticket

volatile ticket_state    my_ticket_lock             = (ticket_state){ .next = 0, .owner = 0};

// statically initialize global data for abql_sharing

volatile uint64_t        queue_last_sharing         = 0;
volatile flag_sharing   *flags_sharing              = NULL; // filled in in tests_multi()

// statically initialize global data for abql_nosharing

volatile uint64_t        queue_last_nosharing       = 0;
volatile flag_nosharing *flags_nosharing            = NULL; // filled in in tests_multi()

// statically initialize global data for mcs_sharing
volatile mcs_sharing     mcs_global_sharing         = (mcs_sharing){ .next = 0, .locked = UNLOCKED };
volatile mcs_sharing    *mcss_sharing               = NULL; // filled in in tests_multi()

// statically initialize global data for mcs_nosharing

volatile mcs_nosharing   mcs_global_nosharing       = (mcs_nosharing){ .next = 0, .locked = UNLOCKED };
volatile mcs_nosharing  *mcss_nosharing             = NULL; // filled in in tests_multi()

// TODO statically initialize global data for coarse_queue (change if you'd
// like to use a lock that's represented by something besides a word
// initialized to UNLOCKED)

volatile coarse_queue    my_coarse_queue            = (coarse_queue){ .head = 0, .tail = 0, .lock = UNLOCKED };

// statically initialize global data for nb_queue

volatile nb_queue        my_nb_queue                = (nb_queue){ .head = 0, .tail = 0 };

uint64_t n_tests = 27;
char *test_names[] = { "spin_try_lock_correctness_nograph"
                     , "spin_lock_correctness_nograph"
                     , "spin_wait_lock_correctness_nograph"
                     , "spin_read_lock_correctness_nograph"
                     , "spin_experimental_lock_correctness_nograph"
                     , "ticket_correctness_nograph"
                     , "abql_sharing_correctness_nograph"
                     , "abql_nosharing_correctness_nograph"
                     , "mcs_sharing_correctness_nograph"
                     , "mcs_nosharing_correctness_nograph"
                     , "coarse_queue_correctness_nograph"
                     , "nb_queue_correctness_nograph"

                     , "pthread_spin_lock"

                     , "spin_try_lock_nograph"
                     , "spin_lock" 
                     , "spin_wait_lock"
                     , "spin_read_lock"
                     , "spin_experimental_lock"
                     , "ticket_lock"
                     , "abql_sharing_lock"
                     , "abql_nosharing_lock"
                     , "mcs_sharing_lock"
                     , "mcs_nosharing_lock"
                     , "coarse_queue"
                     , "coarse_queue_enq"
                     , "nb_queue"
                     , "nb_queue_enq"
                     };

// skip tests with 0s, each value corresponds to the test in the same position
// in test_names above
uint64_t test_on[] = { 0 // spin_try_lock_correctness_nograph
                     , 0 // spin_lock_correctness_nograph
                     , 0 // spin_wait_lock_correctness_nograph
                     , 0 // spin_read_lock_correctness_nograph
                     , 0 // spin_experimental_lock_correctness_nograph
                     , 0 // ticket_correctness_nograph
                     , 0 // abql_sharing_correctness_nograph
                     , 0 // abql_nosharing_correctness_nograph
                     , 0 // mcs_sharing_correctness_nograph
                     , 0 // mcs_nosharing_correctness_nograph
                     , 1 // coarse_queue_correctness_nograph
                     , 0 // nb_queue_correctness_nograph

                     , 0 // pthread_spin_lock

                     , 0 // spin_try_lock_nograph
                     , 0 // spin_lock 
                     , 0 // spin_wait_lock
                     , 0 // spin_read_lock
                     , 0 // spin_experimental_lock
                     , 0 // ticket_lock
                     , 0 // abql_sharing_lock
                     , 0 // abql_nosharing_lock
                     , 0 // mcs_sharing_lock
                     , 0 // mcs_nosharing_lock
                     , 0 // coarse_queue
                     , 0 // coarse_queue_enq
                     , 0 // nb_queue
                     , 0 // nb_queue_enq
                     };

uint64_t accesses_on[] = { 1 // spin_try_lock_correctness_nograph
                         , 1 // spin_lock_correctness_nograph
                         , 1 // spin_wait_lock_correctness_nograph
                         , 1 // spin_read_lock_correctness_nograph
                         , 1 // spin_experimental_lock_correctness_nograph
                         , 1 // ticket_correctness_nograph
                         , 1 // abql_sharing_correctness_nograph
                         , 1 // abql_nosharing_correctness_nograph
                         , 1 // mcs_sharing_correctness_nograph
                         , 1 // mcs_nosharing_correctness_nograph
                         , 0 // coarse_queue_correctness_nograph
                         , 0 // nb_queue_correctness_nograph

                         , 1 // pthread_spin_lock

                         , 1 // spin_try_lock_nograph
                         , 1 // spin_lock 
                         , 1 // spin_wait_lock
                         , 1 // spin_read_lock
                         , 1 // spin_experimental_lock
                         , 1 // ticket_lock
                         , 1 // abql_sharing_lock
                         , 1 // abql_nosharing_lock
                         , 1 // mcs_sharing_lock
                         , 1 // mcs_nosharing_lock
                         , 0 // coarse_queue
                         , 0 // coarse_queue_enq
                         , 0 // nb_queue
                         , 0 // nb_queue_enq
                         };
op **oss = NULL;
uint64_t n_accesses = 3;
uint64_t accessesv[] = { 0    // no accesses
                       , CACHE_LINE    
                       , 10 * CACHE_LINE
                       //, L1_DATA_CACHE 
                       };

uint64_t coarse_count(volatile coarse_queue *queue) { 
  uint64_t n = 0;
  uint64_t head = CMM_LOAD_SHARED(queue->head);

  if(head != 0) {
    while(head != 0) {
      ++n;
      head = CMM_LOAD_SHARED(((coarse_node*)get_qptr(head))->next);
    }
  }

  return n;
}

uint64_t nb_count(volatile nb_queue *queue) { 
  uint64_t n = 0;
  uint64_t head = CMM_LOAD_SHARED(queue->head);

  if(head != 0) {
    while(head != 0) {
      ++n;
      head = CMM_LOAD_SHARED(((nb_node*)get_qptr(head))->next);
    }
    --n; // account for dummy node
  }

  return n;
}

void tests_multi() {
  int i,j,k,all_ops,test;
  pthread_t ts[n_threads];
  ti_data_in datas[n_threads];
  global_datas = datas;
  oss = malloc(sizeof(op*) * n_tests);
  for(test=0; test<n_tests; ++test) { oss[test] = malloc(sizeof(op) * n_ops); }
  global_dequeues = (uint64_t*)malloc(sizeof(uint64_t) * n_ops); // probably just need half of this
  op *cur, *cur1;
  uint64_t tsc,sum;
  unsigned int status;
  int nt,o,ret,run,accesses;
  uint64_t total_enqueues, total_dequeues, remaining_in_queue;

  // GLOBAL DATA (dynamically determined initial values)

  // initialize allocation pool
  global_pool_meta = malloc(n_pool_lines * sizeof(pool_meta));
  // 8 bytes per word
  global_pool_data = aligned_alloc(CACHE_LINE_BYTES, n_pool_lines * CACHE_LINE_BYTES);

  // TODO declare and initialize data for abql_sharing
  volatile flag_sharing flags_local_sharing[n_threads];
  flags_local_sharing[0].val = HAS_LOCK;
  for(i=1; i<n_threads; ++i) { flags_local_sharing[i].val = MUST_WAIT; }
  flags_sharing = flags_local_sharing;

  // TODO declare and initialize data for abql_nosharing
  volatile flag_nosharing flags_local_nosharing[n_threads] __attribute__((aligned (CACHE_LINE_BYTES)));
  flags_local_nosharing[0].val = HAS_LOCK;
  for(i=1; i<n_threads; ++i) { flags_local_nosharing[i].val = MUST_WAIT; }
  flags_nosharing = flags_local_nosharing;

  // TODO declare and initialize array for mcs with sharing
  volatile mcs_sharing mcss_local_sharing[n_threads];
  for(i=0; i<n_threads; ++i) {
    mcss_local_sharing[i].next = 0;
    mcss_local_sharing[i].locked = UNLOCKED;
  }
  mcss_sharing = mcss_local_sharing;

  // TODO declare and initialize array for mcs with nosharing
  volatile mcs_nosharing mcss_local_nosharing[n_threads];
  for(i=0; i<n_threads; ++i) {
    mcss_local_nosharing[i].next = 0;
    mcss_local_nosharing[i].locked = UNLOCKED;
  }
  mcss_nosharing = mcss_local_nosharing;

  // header for csv
  printf_csv("ops,test,run,nt,critical_section_accesses,ticks\n","");

  pthread_spin_init(&pthreads_spinlock, 0);

  // set up workers to initialize the set of operations in parallel
  for(i=0; i<n_threads; ++i) {
    datas[i].i                   = i;
    datas[i].phase               = INIT_PHASE;
    datas[i].my_spinlock_counter = 0;
    tsc                          = __rdtscp(&status);
    srand48_r((long int)tsc + i, &datas[i].rand_state);
    datas[i].buffer_cur          = (ACCESS_BUFFER_SIZE / n_threads) * i;
    datas[i].buffer              = global_buffer;
    datas[i].ti_dequeues_curs    = malloc(n_threads * sizeof(uint64_t));
  } 

  // guarantee that writes to data fields come before threads start running.
  // precautionary, set_affinity() and pthread_create() probably contain atomic
  // instructions that act as full barriers. 
  cmm_smp_wmb();
  
  // set parent affinity so it's not sharing a CPU with a worker thread
  set_affinity();

  // spawn workers
  for(i=0; i<n_threads; ++i) { pthread_create(&ts[i], NULL, ti, &datas[i]); }

  for(test=0; test < n_tests; ++test) {
    if(!test_on[test]) { continue; }
    printf_parent("-1: initializing test %s\n", test_names[test]);
    for(run=0; run < n_runs; ++run) {
      // wait for workers to finish initialization
      parent_spin(datas, READY_PHASE, -1);
      parent_announce(READY_PHASE, -1);
      
      // see how long it takes each number of threads to solve 
      // the problem we've initialized
      for(nt=1; nt<=n_threads; ++nt) {
        // try different numbers of memory accesses in the critical section
        for(accesses=0; accesses < (accesses_on[test] ? n_accesses : 1); ++accesses) {
          printf_parent("-1: starting loop for test %s, nt %d\n, accesses %" PRIu64 "\n", test_names[test], nt, accessesv[accesses]);

          // global per-run initialization
          switch(test) {
            case COARSE_QUEUE_CORRECTNESS_TEST:
            case COARSE_QUEUE_TEST:
            case COARSE_QUEUE_ENQ_TEST:
              // initialize coarse_queue state for each run
              _CMM_STORE_SHARED(my_coarse_queue.head, 0);
              _CMM_STORE_SHARED(my_coarse_queue.tail, 0);
              // TODO initialize lock state for each run
              _CMM_STORE_SHARED(my_coarse_queue.lock, UNLOCKED);
              break;
            case NB_QUEUE_CORRECTNESS_TEST:
            case NB_QUEUE_TEST:
            case NB_QUEUE_ENQ_TEST:
              // initialize nb_queue state for each run
              _CMM_STORE_SHARED(my_nb_queue.head, 0);
              _CMM_STORE_SHARED(my_nb_queue.tail, 0);
              break;
          }

          parent_spin(datas, WARMED_PHASE, -1);

          // time from the parent's perspective from when parent tells workers to
          // start OPS_PHASE until parent observes that all workers have reported
          // back that they have completed OPS_PHASE
          TIME_AND_CHECK(1,0,run,parent_announce_then_spin(datas, WARMED_PHASE, OPS_PHASE, -1), "OPS_PHASE_TIME", tsc, ret);

          // tell workers to start their checking pass
          parent_announce(OPS_PHASE, -1);

          // wait for workers to complete checking pass before running parent checks
          parent_spin(datas, CHECK_PHASE, -1);

          // if applicable, check results of run and reset test state
          all_ops = 1;
          switch (test) {
            case SPIN_TRY_LOCK_CORRECTNESS_TEST:
              all_ops = 0;
            case SPIN_LOCK_CORRECTNESS_TEST:
            case SPIN_WAIT_LOCK_CORRECTNESS_TEST:
            case SPIN_READ_LOCK_CORRECTNESS_TEST:
            case SPIN_EXPERIMENTAL_LOCK_CORRECTNESS_TEST:
            case ABQL_SHARING_CORRECTNESS_TEST:
            case ABQL_NOSHARING_CORRECTNESS_TEST:
            case MCS_SHARING_CORRECTNESS_TEST:
            case MCS_NOSHARING_CORRECTNESS_TEST:
            case TICKET_CORRECTNESS_TEST:
              // see if per-thread counters sum to shared lock-protected counter
              sum = 0;
              for(i=0; i<nt; ++i) { sum += datas[i].my_spinlock_counter; }
              if (sum == 0 || sum != my_spinlock_shared_counter || (all_ops && sum != n_ops)) {
                if (test == SPIN_TRY_LOCK_CORRECTNESS_TEST) {
                  printf_error("-1: %s failed for nt %d, my_spinlock_shared_counter: %" PRIu64 
                               ", sum: %" PRIu64 ", n_ops: %" PRIu64
                               ".  If the lock were working correctly, counter and sum would "
                               "be nonzero and the same.\n"
                               , test_names[test], nt, my_spinlock_shared_counter, sum, n_ops);
                }
                else {
                  printf_error("-1: %s failed for nt %d, my_spinlock_shared_counter: %" PRIu64 
                               ", sum: %" PRIu64 ", n_ops: %" PRIu64
                               ".  If the lock were working correctly, they would all "
                               "be nonzero and the same.\n"
                               , test_names[test], nt, my_spinlock_shared_counter, sum, n_ops);
                }
                exit(-1);
              }
              else {
                printf_parent("-1: %s succeeded for nt %d, my_spinlock_shared_counter: %" PRIu64 
                              ", sum: %" PRIu64 ", n_ops: %" PRIu64
                              ".  Values are nonzero and the same, so the lock is working correctly.\n"
                             , test_names[test], nt, my_spinlock_shared_counter, sum, n_ops);
              }
              // reset counters for next test
              my_spinlock_shared_counter = 0;
              for(i=0; i<nt; ++i) { datas[i].my_spinlock_counter = 0; }
              break;
            case COARSE_QUEUE_CORRECTNESS_TEST:
            case NB_QUEUE_CORRECTNESS_TEST:
              switch (test) {
                case COARSE_QUEUE_CORRECTNESS_TEST:
                  remaining_in_queue = coarse_count(&my_coarse_queue);
                  break;
                case NB_QUEUE_CORRECTNESS_TEST:
                  remaining_in_queue = nb_count(&my_nb_queue);
                  break;
              }
              total_enqueues = 0;
              total_dequeues = 0;
              for(i=0; i<n_threads; ++i) {
                total_enqueues += datas[i].n_enqueues;
              }
              for(i=0; i<nt; ++i) {
                total_dequeues += datas[i].n_dequeues;
              }
              if(total_enqueues == total_dequeues + remaining_in_queue) {
                printf_parent("-1: %s succeeded for nt %d.  Total enqueues equals total dequeues "
                              "plus nodes remaining in queue. No worker's enqueues were dequeued out "
                              "of order, so the queue is FIFO.\n", test_names[test], nt);
              }
              else {
                printf_error("-1: %s failed for nt %d.  Total enqueues (%" PRIu64 ") is not equal to "
                             "total dequeues (%" PRIu64 ") plus nodes remaining in queue (%" PRIu64 "), "
                             "so we must have dropped some enqueues or dequeued some nodes more than once.\n"
                             , test_names[test], nt, total_enqueues, total_dequeues, remaining_in_queue);
                exit(-1);
              }
              break;
            default: 
              break;
          }

          // send workers back to the top of their loop
          parent_announce(CHECK_PHASE, -1);

          // output results for this nt
          printf_csv("%" PRIu64 ",%s,%d,%d,%" PRIu64 ",%" PRIu64 "\n",n_ops,test_names[test],run,nt,accessesv[accesses],tsc);
        }
      }
    }
    parent_spin(datas, INIT_PHASE, -1);
    parent_announce(INIT_PHASE, -1);
  }

  // join workers
  for(i=0; i<n_threads; ++i) { pthread_join(ts[i], NULL); }

  // clean up
  for(test=0; test<n_tests; ++test) { free(oss[test]); }
  for(i=0; i<n_threads; ++i) { free(datas[i].ti_dequeues_curs); }
  free(oss);
  free(global_pool_data);
  free(global_pool_meta);
  free(global_dequeues);
  pthread_spin_destroy(&pthreads_spinlock);
}

void parent_announce(uint64_t phase, int t_i)
{
  _CMM_STORE_SHARED(completed_phase, phase);
  printf_parent("%d: announced start of phase %" PRIu64 "\n", t_i, phase);
}

void parent_spin(ti_data_in *datas, uint64_t phase, int t_i)
{
  for(int i=0; i<n_threads; ++i) {
    spin_until(&(datas[i].phase), phase, t_i);
  }
}

int parent_announce_then_spin(ti_data_in *datas, uint64_t phase1, uint64_t phase2, int t_i)
{
  parent_announce(phase1, t_i);

  parent_spin(datas, phase2, t_i);

  return 0;
}
