#include "util.h"

unsigned int next_aff = 0;

pthread_mutex_t affinity_mutex = PTHREAD_MUTEX_INITIALIZER;

void set_affinity(void)
{
	cpu_set_t mask;
	int cpu;
	int ret;

	ret = pthread_mutex_lock(&affinity_mutex);
	if (ret) {
		perror("Error in pthread mutex lock");
		exit(-1);
	}

  if(next_aff >= n_cpus) {
    perror("Ran out of CPUs, reduce n_threads");
    exit(-1);
  }
	cpu = next_aff++;
	ret = pthread_mutex_unlock(&affinity_mutex);
	if (ret) {
		perror("Error in pthread mutex unlock");
		exit(-1);
	}
	CPU_ZERO(&mask);
	CPU_SET(cpu, &mask);
	sched_setaffinity(0, sizeof(mask), &mask);
#if defined(PRINT_AFFINITIES)
  printf_verbose("set affinity %d\n", cpu);
#endif
}

#define in_pool(line) (global_pool_data <= (line) && (line) <= global_pool_data + (n_pool_lines - 1) * CACHE_LINE)

uint64_t pool_allocate(ti_data_in *d, uint64_t *p) {
  uint64_t i = d->pool_cur;
  uint64_t addr, ret, seq;
  do {
    if (CMM_LOAD_SHARED(d->pool_meta[i].status) == FREE) {
      _CMM_STORE_SHARED(d->pool_meta[i].status, USED);
      d->pool_cur = (i + 1) % d->pool_lines;
      addr = (uint64_t)(d->pool_data + i * CACHE_LINE);
      seq = d->pool_meta[i].sequence_number;
      if (addr % CACHE_LINE_BYTES != 0) {
        printf_error("%d: Allocator isn't giving back cache-line-aligned addresses.  Tried to return %" PRIx64 ", sequence number %" PRIu64 "\n",d->i,addr,seq);
        while(1);
      }
      if (!in_pool((uint64_t*)get_qptr(addr))) {
        printf_error("%d: Why am I allocating memory outside the pool?  Tried to return %" PRIx64 "\n",d->i,addr);
        while(1);
      }
      ret = addr | seq;
      //printf("%d: allocated %" PRIx64 ", sequence number %" PRIu64 "\n", d->i, addr, d->pool_meta[i].sequence_number);
      _CMM_STORE_SHARED(*p, ret);
      // TODO why is returning uint64_t broken?  It sets the top half of the word for no reason,
      // works fine if I instead store to an address as above.
      return 1;
    }
    i = (i + 1) % d->pool_lines;
  } while (i != d->pool_cur);

  printf_error("%d: Ran out of memory in my allocation pool, crank up pool size or reduce number of operations.\n", d->i);
  exit(-1);
}

// this is okay, because the freer will always be unique (nobody else dequeued
// that node), and the allocation happens in enqueue, which will always happen
// before the corresponding dequeue.
void pool_free(ti_data_in *d, uint64_t *line) {
  if(!in_pool(line)) {
    printf_error("%d: Tried to free a line %" PRIx64 " not from the pool, failing.\n", d->i, (uint64_t)line); 
    exit(-1);
  }

  uint64_t i = (uint64_t)(line - global_pool_data) / CACHE_LINE;
  if(global_pool_data + i * CACHE_LINE != line) { 
    printf_error("%d: pool_free isn't lining up lines right\n",d->i); 
    while(1); 
  }
  if(CMM_LOAD_SHARED(global_pool_meta[i].status) != USED) {
    printf_error("%d: Tried to free a line %" PRIx64 " not in use, failing.\n", d->i, (uint64_t)line);
    while(1);
    exit(-1);
  }

  uint64_t sequence_number = global_pool_meta[i].sequence_number;
  if (sequence_number >= 63) {
    _CMM_STORE_SHARED(global_pool_meta[i].status, DEAD);
  } 
  else {
    _CMM_STORE_SHARED(global_pool_meta[i].sequence_number, sequence_number + 1);
    // atomically update status at the end, so the next allocator finds the
    // correct sequence number.
    _CMM_STORE_SHARED(global_pool_meta[i].status, FREE);
  }

  return;
}
