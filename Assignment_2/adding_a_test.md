1.  Update test_names and increment n_tests in tests.c.
2.  Add any applicable new operations (with unique numbers) under "// OPERATIONS" in tests.h.
3.  Add a test (with a unique number) under "// TESTS" in tests.h.
4.  Add declarations for any applicable global data in tests.h under "// GLOBAL DATA" and initialize as needed under "// GLOBAL DATA" in tests.c, (or in tests_multi() in tests.c, if the initialization depends on runtime parameters).
5.  Add any new per-thread data to ti_data_in in util.h
6.  Add a test case under "// initialize test data" in worker.c to initialize the test data in parallel.
7.  Add any needed per-test-run initialization under "// per-test-run initialization" in worker.c.
8.  Add a test case under "// warm the cache" in worker.c.
9.  Add cases for new operations under "// enter critical section" in worker.c.
10. Add cases for new operations under "// critical section body" (if applicable) in worker.c.
11. Add cases for new operations under "// leave critical section" in worker.c.
12. If parent needs to do some checking/work after each run, add a test case under "// if applicable, check results of run and reset test state" in tests.c

