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

