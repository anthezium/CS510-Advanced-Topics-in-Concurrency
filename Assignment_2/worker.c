#include "tests.h"
#include "util.h"
#include "worker.h"

void announce(volatile uint64_t *p, uint64_t phase, int t_i) __attribute__((always_inline));
void announce_then_spin(volatile uint64_t *p, uint64_t phase, int t_i) __attribute__((always_inline));

void* ti(void *data) {
  int                i, m, tryret, ret, run, nt, si, ei, test, accesses;
  op                 *cur, *start, *end;
  ti_data_in         *d = (ti_data_in*)data;
  pthread_spinlock_t pthreads_spinlock_copy;
  uint64_t           my_spinlock_copy;
  volatile uint64_t  my_place;
  op                 op_copy;
  mcs_sharing        my_mcs_sharing_copy;
  mcs_nosharing      my_mcs_nosharing_copy;
  ticket_state        my_ticket_lock_copy;
  
  // get this thread running on its own CPU
  set_affinity();

  for(test=0; test<n_tests; ++test) {
    if(!test_on[test]) { continue; }
    for(run=0; run<n_runs; ++run) {
      // initialize test data
      m   = n_ops / n_threads;
      end = ( d->i == n_threads - 1 ? oss[test] + n_ops : oss[test] + (d->i + 1) * m);
      switch (test) {
        case SPIN_TRY_LOCK_CORRECTNESS_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = SPIN_TRY_LOCK_INC_OP }));
					}
          break;
        case SPIN_LOCK_CORRECTNESS_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = SPIN_LOCK_INC_OP }));
					}
          break;
        case SPIN_WAIT_LOCK_CORRECTNESS_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = SPIN_WAIT_LOCK_INC_OP }));
					}
          break;
        case SPIN_READ_LOCK_CORRECTNESS_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = SPIN_READ_LOCK_INC_OP }));
					}
          break;
        case SPIN_EXPERIMENTAL_LOCK_CORRECTNESS_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = SPIN_EXPERIMENTAL_LOCK_INC_OP }));
					}
          break;
        case ABQL_SHARING_CORRECTNESS_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = ABQL_SHARING_INC_OP }));
					}
          break;
        case ABQL_NOSHARING_CORRECTNESS_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = ABQL_NOSHARING_INC_OP }));
					}
          break;
        case MCS_SHARING_CORRECTNESS_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = MCS_SHARING_INC_OP }));
					}
          break;
        case MCS_NOSHARING_CORRECTNESS_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = MCS_NOSHARING_INC_OP }));
					}
          break;
        case TICKET_CORRECTNESS_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = TICKET_INC_OP }));
					}
          break;
        case PTHREAD_SPIN_LOCK_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = PTHREAD_SPIN_LOCK_LOCK_OP }));
					}
          break;
        case SPIN_TRY_LOCK_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = SPIN_TRY_LOCK_LOCK_OP }));
					}
          break;
        case SPIN_LOCK_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = SPIN_LOCK_LOCK_OP }));
					}
          break;
        case SPIN_WAIT_LOCK_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = SPIN_WAIT_LOCK_LOCK_OP }));
					}
          break;
        case SPIN_READ_LOCK_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = SPIN_READ_LOCK_LOCK_OP }));
					}
          break;
        case SPIN_EXPERIMENTAL_LOCK_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = SPIN_EXPERIMENTAL_LOCK_LOCK_OP }));
					}
          break;
        case ABQL_SHARING_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = ABQL_SHARING_LOCK_OP }));
					}
          break;
        case ABQL_NOSHARING_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = ABQL_NOSHARING_LOCK_OP }));
					}
          break;
        case MCS_SHARING_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = MCS_SHARING_LOCK_OP }));
					}
          break;
        case MCS_NOSHARING_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = MCS_NOSHARING_LOCK_OP }));
					}
          break;
        case TICKET_TEST:
					for(cur = oss[test] + d->i * m; cur<end; ++cur) {
						_CMM_STORE_SHARED(*cur, ((op){ .operation = TICKET_LOCK_OP }));
					}
          break;
        default: 
          printf("Undefined test %d\n", test);
          break;
      }
      // let the parent know we're done and wait for parent to say that everyone
      // else is done
      announce_then_spin(&d->phase, READY_PHASE, d->i);

      // run the test for each number of threads
      for(nt=1; nt<=n_threads; ++nt) {
        for(accesses=0; accesses<n_accesses; ++accesses) {
          if(d->i > nt - 1) {
            // this worker is inactive for this nt, just hang out without consuming
            // a lot of resources

            printf_worker("%d: nt is %d, skipping this round\n", d->i, nt);

            // no warming to do, say I'm done and wait for parent to say that everyone
            // else is done
            announce(&d->phase, WARMED_PHASE, d->i);
            //sleep_until(&completed_phase, WARMED_PHASE, d->i);
            spin_until(&completed_phase, WARMED_PHASE, d->i);

            // no ops to do, say I'm done and wait for parent to say that everyone
            // else is done
            announce(&d->phase, OPS_PHASE, d->i);
            //sleep_until(&completed_phase, OPS_PHASE, d->i);
            spin_until(&completed_phase, OPS_PHASE, d->i);
          }
          else {
            // this worker is active for this nt, let's run some tests!

            // per-test-run initialization
            m     = n_ops / nt;
            si    = d->i * m;
            start = oss[test] + si;
            ei    = si + m + ( d->i == nt - 1 ? n_ops - si - m : 0 ) - 1;
            end   = oss[test] + ei;

            printf_worker("%d: nt is %d, running ops %d-%d [0x%" PRIx64 "-0x%" PRIx64 "]\n", d->i, nt, si, ei, (uint64_t)start, (uint64_t)end);

            // warm the cache
            // TODO does warming the ops help?
            for(cur = start; cur <= end; ++cur) { op_copy = *cur; }

            switch (test) {
              case PTHREAD_SPIN_LOCK_TEST:
				  			pthreads_spinlock_copy = pthreads_spinlock;
                break;
              case SPIN_TRY_LOCK_CORRECTNESS_TEST:
              case SPIN_LOCK_CORRECTNESS_TEST:
              case SPIN_WAIT_LOCK_CORRECTNESS_TEST:
              case SPIN_READ_LOCK_CORRECTNESS_TEST:
              case SPIN_EXPERIMENTAL_LOCK_CORRECTNESS_TEST:
              case SPIN_TRY_LOCK_TEST:
              case SPIN_LOCK_TEST:
              case SPIN_WAIT_LOCK_TEST:
              case SPIN_READ_LOCK_TEST:
              case SPIN_EXPERIMENTAL_LOCK_TEST:
                my_spinlock_copy = my_spinlock;
                break;
              case ABQL_SHARING_CORRECTNESS_TEST:
              case ABQL_SHARING_TEST:
                for(i=0; i<n_threads; ++i) { my_spinlock_copy = flags_sharing[i].val; }
                break;
              case ABQL_NOSHARING_CORRECTNESS_TEST:
              case ABQL_NOSHARING_TEST:
                for(i=0; i<n_threads; ++i) { my_spinlock_copy = flags_nosharing[i].val; }
                break;
              case MCS_SHARING_CORRECTNESS_TEST:
              case MCS_SHARING_TEST:
                for(i=0; i<n_threads; ++i) { my_mcs_sharing_copy = mcss_sharing[i]; }
                break;
              case MCS_NOSHARING_CORRECTNESS_TEST:
              case MCS_NOSHARING_TEST:
                for(i=0; i<n_threads; ++i) { my_mcs_nosharing_copy = mcss_nosharing[i]; }
                break;
              case TICKET_CORRECTNESS_TEST:
              case TICKET_TEST:
                my_ticket_lock_copy = my_ticket_lock;
                break;
              default:
				  			printf("Undefined test %d\n", test);
				  			break;
            }

            cmm_smp_mb(); // make sure warming loads happen before announcement

            announce_then_spin(&d->phase, WARMED_PHASE, d->i);

            // run the ops!
            for(cur = start, i=si; cur <= end; ++cur, ++i) {
              printf_worker("%d: running op %d\n", d->i, i);

              // enter critical section
              switch (cur->operation) {
                case PTHREAD_SPIN_LOCK_LOCK_OP:
                  pthread_spin_lock(&pthreads_spinlock);
                  break;
                case SPIN_TRY_LOCK_LOCK_OP:
                case SPIN_TRY_LOCK_INC_OP:
                  tryret = spin_try_lock(&my_spinlock);
                  break;
                case SPIN_LOCK_LOCK_OP:
                case SPIN_LOCK_INC_OP:
                  spin_lock(&my_spinlock);
                  break;
                case SPIN_WAIT_LOCK_LOCK_OP:
                case SPIN_WAIT_LOCK_INC_OP:
                  spin_wait_lock(&my_spinlock);
                  break;
                case SPIN_READ_LOCK_LOCK_OP:
                case SPIN_READ_LOCK_INC_OP:
                  spin_read_lock(&my_spinlock);
                  break;
                case SPIN_EXPERIMENTAL_LOCK_LOCK_OP:
                case SPIN_EXPERIMENTAL_LOCK_INC_OP:
                  spin_experimental_lock(&my_spinlock);
                  break;
                case ABQL_SHARING_LOCK_OP:
                case ABQL_SHARING_INC_OP:
                  abql_sharing_lock(&my_place, &queue_last_sharing, flags_sharing, n_threads);
                  break;
                case ABQL_NOSHARING_LOCK_OP:
                case ABQL_NOSHARING_INC_OP:
                  abql_nosharing_lock(&my_place, &queue_last_nosharing, flags_nosharing, n_threads);
                  break;
                case MCS_SHARING_LOCK_OP:
                case MCS_SHARING_INC_OP:
                  mcs_sharing_lock(&mcs_global_sharing, &mcss_sharing[d->i]);
                  break;
                case MCS_NOSHARING_LOCK_OP:
                case MCS_NOSHARING_INC_OP:
                  mcs_nosharing_lock(&mcs_global_nosharing, &mcss_nosharing[d->i]);
                  break;
                case TICKET_LOCK_OP:
                case TICKET_INC_OP:
                  ticket_lock(&my_ticket_lock);
                  break;
                default:
                  printf_error("Undefined operation %" PRIu64 "\n", cur->operation);
                  exit(-1);
                  break;
              }

              // critical section body

              // * simulate work by accessing memory
              access(i, d, accessesv[accesses]);

              // * do per-operation critical section work
              switch(cur->operation) {
                case SPIN_LOCK_INC_OP:
                case SPIN_WAIT_LOCK_INC_OP:
                case SPIN_READ_LOCK_INC_OP:
                case SPIN_EXPERIMENTAL_LOCK_INC_OP:
                case ABQL_SHARING_INC_OP:
                case ABQL_NOSHARING_INC_OP:
                case MCS_SHARING_INC_OP:
                case MCS_NOSHARING_INC_OP:
                case TICKET_INC_OP:
                  ++my_spinlock_shared_counter;
                  ++d->my_spinlock_counter;
                  break;
                case SPIN_TRY_LOCK_INC_OP:
                  if (tryret) {
                    ++my_spinlock_shared_counter;
                    ++d->my_spinlock_counter;
                  }
                  break;
              }

              // leave critical section
              switch (cur->operation) {
                case PTHREAD_SPIN_LOCK_LOCK_OP:
                  pthread_spin_unlock(&pthreads_spinlock);
                  break;
                case SPIN_LOCK_LOCK_OP:
                case SPIN_LOCK_INC_OP:
                case SPIN_WAIT_LOCK_LOCK_OP:
                case SPIN_WAIT_LOCK_INC_OP:
                case SPIN_READ_LOCK_LOCK_OP:
                case SPIN_READ_LOCK_INC_OP:
                case SPIN_EXPERIMENTAL_LOCK_LOCK_OP:
                case SPIN_EXPERIMENTAL_LOCK_INC_OP:
                  spin_unlock(&my_spinlock);
                  break;
                case SPIN_TRY_LOCK_LOCK_OP:
                case SPIN_TRY_LOCK_INC_OP:
                  if(tryret) { spin_unlock(&my_spinlock); }
                  break;
                case ABQL_SHARING_LOCK_OP:
                case ABQL_SHARING_INC_OP:
                  abql_sharing_unlock(&my_place, flags_sharing, n_threads);
                  break;
                case ABQL_NOSHARING_LOCK_OP:
                case ABQL_NOSHARING_INC_OP:
                  abql_nosharing_unlock(&my_place, flags_nosharing, n_threads);
                  break;
                case MCS_SHARING_LOCK_OP:
                case MCS_SHARING_INC_OP:
                  mcs_sharing_unlock(&mcs_global_sharing, &mcss_sharing[d->i]);
                  break;
                case MCS_NOSHARING_LOCK_OP:
                case MCS_NOSHARING_INC_OP:
                  mcs_nosharing_unlock(&mcs_global_nosharing, &mcss_nosharing[d->i]);
                  break;
                case TICKET_LOCK_OP:
                case TICKET_INC_OP:
                  ticket_unlock(&my_ticket_lock);
                  break;
              }

            }

            // I'm done with my ops, say so and wait for parent to say that
            // everyone else is done
            announce_then_spin(&d->phase, OPS_PHASE, d->i);
          }
        }
      }
    }
    announce_then_spin(&d->phase, INIT_PHASE, d->i);
  }

  return data;
}

// spin_try_lock
int spin_try_lock(volatile uint64_t *lock) {
  // TODO
}

// spin_lock
void spin_lock(volatile uint64_t *lock) {
  // TODO
}

// spin_wait_lock
void spin_wait_lock(volatile uint64_t *lock) {
  // TODO
}

// spin_read_lock
void spin_read_lock(volatile uint64_t *lock) {
 // TODO
}

// spin_experimental_lock
void spin_experimental_lock(volatile uint64_t *lock) {
 // TODO
}
    

void spin_unlock(volatile uint64_t *lock) {
  // TODO
}

// BEGIN ASSIGNMENT SECTION

void ticket_lock(volatile ticket_state *lock) {
  // TODO
}

void ticket_unlock(volatile ticket_state *lock) {
  // TODO
}

void abql_sharing_lock(volatile uint64_t *my_place, volatile uint64_t *queue_last, 
                       volatile flag_sharing *flags, uint64_t n_threads) {
  // TODO
}

void abql_nosharing_lock(volatile uint64_t *my_place, volatile uint64_t *queue_last, 
                         volatile flag_nosharing *flags, uint64_t n_threads) {
  // TODO
}

void abql_sharing_unlock(volatile uint64_t *my_place, volatile flag_sharing *flags, uint64_t n_threads) {
  // TODO
}

void abql_nosharing_unlock(volatile uint64_t *my_place, volatile flag_nosharing *flags, uint64_t n_threads) {
  // TODO
}

void mcs_sharing_lock(volatile mcs_sharing *global_lock, volatile mcs_sharing *local_lock) {
  // TODO
}

void mcs_nosharing_lock(volatile mcs_nosharing *global_lock, volatile mcs_nosharing *local_lock) {
  // TODO
}

void mcs_sharing_unlock(volatile mcs_sharing *global_lock, volatile mcs_sharing *local_lock) {
  // TODO
}

void mcs_nosharing_unlock(volatile mcs_nosharing *global_lock, volatile mcs_nosharing *local_lock) 
{
  // TODO
}

// END ASSIGNMENT SECTION

void announce(volatile uint64_t *p, uint64_t phase, int t_i)
{
  _CMM_STORE_SHARED(*p, phase);

  cmm_smp_mb(); // necessary?

  printf_worker("%d: announced finished with phase %" PRIu64 "\n", t_i, phase);
}

void announce_then_spin(volatile uint64_t *p, uint64_t phase, int t_i)
{
  announce(p, phase, t_i);
  spin_until(&completed_phase, phase, t_i);
}

