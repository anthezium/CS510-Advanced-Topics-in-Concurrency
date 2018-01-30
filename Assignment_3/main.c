#include <inttypes.h>
#include <stdio.h>

#include "tests.h"
#include "util.h"

uint64_t n_threads, n_cpus, n_runs, n_ops, n_pool_lines; 

void show_usage(int argc, char **argv)
{
	printf("Usage : %s N_THREADS N_CPUS N_OPS N_RUNS N_POOL_LINES\n", argv[0]);
}

int main(int argc, char** argv) {
  int err, ret;

	if (argc != 6) {
		show_usage(argc, argv);
		return -1;
	}

	err = sscanf(argv[1], "%" PRIu64, &n_threads);
	if (err != 1) {
		show_usage(argc, argv);
		return -1;
	}

	err = sscanf(argv[2], "%" PRIu64, &n_cpus);
	if (err != 1) {
		show_usage(argc, argv);
		return -1;
	}

	err = sscanf(argv[3], "%" PRIu64, &n_ops);
	if (err != 1) {
		show_usage(argc, argv);
		return -1;
	}

	err = sscanf(argv[4], "%" PRIu64, &n_runs);
	if (err != 1) {
		show_usage(argc, argv);
		return -1;
	}

	err = sscanf(argv[5], "%" PRIu64, &n_pool_lines);
	if (err != 1) {
		show_usage(argc, argv);
		return -1;
	}

  if (n_ops < n_threads) {
    printf("error: N_OPS < N_THREADS.  We need at least 1 operation per thread.\n");
    return -1;
  }


  //tests_single();

  tests_multi();

  return 0;
}
