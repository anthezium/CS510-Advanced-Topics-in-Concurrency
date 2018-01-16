# Assignment 3: FIFO Spinlocks

**Due Date: Tuesday, January 23rd**

## Overview

In this assignment, we're exploring the design space for spinlocks that provide
a fairness property (in addition to solving the critical section problem):
If thread A's acquisition attempt takes effect before thread B's acquisition attempt,
thread A will get the lock before thread B.  In all the implementations we'll explore,
the is achieved by tying the order in which threads acquire the lock to the coherence
order for a word in memory.

The series of steps in this assignment build on one-another only conceptually,
so can complete the implementations in any order to get things working (just
compare all of them once you're done).  There are numbered questions
throughout the assignment; we recommend thinking through these questions
carefully and writing down short concrete answers before moving on to the next
section.  If you're confused by a question or don't know where to start, you're
free to discuss or puzzle through them with other students, and Ted and Jon are
also available to help.  The point of these assignments is to develop your
understanding of the material and give you some hands-on intuition for
concurrent programming, and the questions are there to help you understand
what's going on.  If they don't, we'd really like to know, so we can write
some better ones :)

Finally, this assignment is brand new, so there may be gaps in the
documentation and bugs in the code.  Let us know about any issues you find and
we'll resolve them as quickly as possible.

If you have questions and are comfortable sharing them, send them to the
[mailing list](mailto:cs510walpole@cecs.pdx.edu).  If you would like Ted to
privately answer questions or take a look at your code, you can get in touch
with him via [email](mailto:theod@pdx.edu).  He will also make an effort to
lurk on IRC in `#cs510walpole` on irc.cat.pdx.edu (see below for details).

### Submission instructions

```bash
cd CS510-Advanced-Topics-in-Concurrency
pushd Assignment_2
pushd data
# remove all data and graphs BESIDES the final ones you'd like me to look at
rm <out-of-date data and graphs>
popd
make clean
popd
# where NAME is your name separated by underscores
tar czf Assignment_2-NAME.tar.gz Assignment_2/
```
 
Email the archive to [theod@pdx.edu](mailto:theod@pdx.edu) with subject
`Assignment_2-NAME` and any questions and comments you'd like to share.
Include your answers to the numbered questions in the assignment in the text of
the email.

### Why FIFO?

If it's possible for thread A to acquire a lock many times while thread B
continues to wait to acquire the lock, then thread A may end up getting its
work done much sooner, at the expense of thread B's progress.  This lack of
fairness can in extreme cases lead to complete *starvation*, e.g. thread B
continually failing to make any progress.  People who write concurrent programs
observe problems resulting from unfairness in practice.  For example,
Nick Piggin (a Linux contributor) cites some anecdotal evidence of this
(here)[https://lwn.net/Articles/267968/]:

> On an 8 core (2 socket) Opteron, spinlock unfairness is extremely noticable,
> with a userspace test having a difference of up to 2x runtime per thread, and
> some threads are starved or "unfairly" granted the lock up to 1 000 000 (!)
> times.

All of the spinlocks we've implemented so far make no guarantees about
fairness, since they're all based on some mechanism where threads race on some
atomic instruction, and the "winner" acquires the lock while other threads
retry.  If we use a "talking stick" as an analogy for a lock, this is like the
holder throwing the talking stick on the ground once they've finished talking,
and letting everybody who wants to talk dive for it and fight over it.  Once
the dust settles, one winner (and nothing else) is determined, so there's
nothing preventing some people from winning over and over again while others
lose over and over again.  In this assignment, we'll explore a series of
approaches roughly analogous to folks forming a line behind the talking stick
holder, who then (once finished talking) passes the talking stick to the next
person in line.

As you work through the steps, think about what fairness buys us, and when
it is and isn't worthwhile.

### Primitives

To complete some of the following implementations, you'll need a new primitive
that atomically adds to a word in memory and returns its previous value.  We've
provided `x86_64`'s `lock; xaddq` instruction to serve this purpose; the `x` is
for "exchange" (as in `xchgq`).  This primitive is exposed as a macro
`lockxaddq(ptr,val)`, which atomically adds `val` to `*ptr` and returns the
pre-add value of `*ptr`.  Note that unlike the actual `lock; xaddq` instruction, this
macro does not store the old value of `*ptr` in `val`, so you need to catch the
return value.

All previously introduced primitives remain available (and will come in handy),
and are implemented and documented in `util.h`.

To keep the compiler from rearranging our code, e.g. during optimizations, we
need to use some other macros: If another thread might be loading from or
storing to `x`, write `_CMM_STORE_SHARED(x,v);` instead of `x = v;`, and
`CMM_LOAD_SHARED(x)` instead of `x` whenever we load `x`.  For example, if we
were looping on the value of `x`, we would write `while(CMM_LOAD_SHARED(x)) {
... }` instead of `while(x) { ... }`, and if we were loading `x` into a local
variable `y`, we would write `y = CMM_LOAD_SHARED(x)`.

Note that storing an aligned uint64_t with _CMM_STORE_SHARED() is atomic on
x86_64.  It doesn't prevent accesses that would normally be reordered across a
store (only subsequent loads on x86_64) from being reordered across it at
runtime, but if you don't care about that, you can safely use this macro to
unconditionally and atomically update a word that other threads may access.

## Building FIFO Spinlocks

### `ticket_lock()` and `ticket_unlock()`

You can find an overview of ticket locks in 
[this Linux Weekly News article](https://lwn.net/Articles/267968/) 
and a description and example implementation in Section 2.2 and Figure 2, respectively, of 
[Mellor-Crummey and Scott 1991](https://cis.temple.edu/~qzeng/cis5512-fall2015/papers/p21-mellor-crummey.pdf).

Basically, a ticket lock is like when you take a number at the DMV, except
there's only one desk, and it's your job to update the number on display when
you've met with the employee at the desk and finished your business.  It
consists of two numbers that can be separately updated atomically:
* `next`, which is the last number that was handed out to somebody in line
* `owner`, which is the number of the ticket that belongs to the current lock
  holder.
To acquire the lock, you atomically increment `next`, grabbing its previous
value as your number, and then spin until another thread updates `owner` to
match your number (if there's nobody ahead of you in line, this will already be
the case).  Now it's your turn to execute your critical section!

To release the lock, you simply increment `owner`, to let your successor know
that it's their turn.  Note that only the lock holder updates `owner`, so no
other thread will race you here.

You'll need to use `lockxaddq()` (described above) to atomically increment
`next`.

### Testing your `ticket_lock()` and `ticket_unlock()` implementations

As usual, turn on `ticket_correctness_test` to test this implementation
and `ticket_lock` to benchmark it.

Build and run the test harness from a shell:
```bash
make
./run 24 10000 1
```
Note that the correctness test for these implementations (and the others in
this assignment) is only partial: it doesn't test fairness in any way, just
whether or not the implementation solves the critical section problem.  The
benchmarks may shed some light on fairness, since the times indicate how long
it takes _all threads_ to finish their work, so if some threads have been
starved and take longer than others to finish, the overall time goes up.
However, there isn't any way to distinguish between time wasted because of
unfairness and time wasted by e.g. atomic instruction overhead.

Once you're passing this correctness test, it may be helpful to compare the
performance of this implementation to some of your spinlocks from the last
assignment.  In particular, `spin_lock` and `spin_read_lock` (or
`spin_experimental_lock`) establish helpful reference points, and
`pthread_spin_lock` is still a good baseline.

#### Questions

STOPPED HERE

### `spin_lock()`

Once you've convinced yourself that `spin_try_lock()` and `spin_unlock()` are
correct by running some experiments, you'll have seen some messages like

```
-1: spin_try_lock_correctness_nograph succeeded for nt 24,
my_spinlock_shared_counter: 535, sum: 535, n_ops: 10000.  Values are nonzero
and the same, so the lock is working correctly.
```

If only 535 increments actually occurred, then the other 9465 attempts must
have resulted in `spin_try_lock()` returning `0`, that is, a thread observing
that some other thread currently holds `my_spinlock` and therefore failing to
acquire it and perform an increment.  This isn't too surprising, since we have
24 threads competing for `my_spinlock` in this case (note `for nt 24` above).

What we really need is a lock acquisition function that *always works*.  Well,
we could just keep spamming `spin_try_lock()` until it returns `1`, right?
Let's give that a try.  Head over to `void spin_lock(volatile uint64_t *lock)`
in `worker.c`, and replace the `TODO` with some code that calls
`spin_try_lock()` in a tight loop until it returns `1`.  If you'd prefer to use
`lockcmpxchgq()` directly instead of `spin_try_lock()`, that's fine too.

This code doesn't return until the lock is acquired; it just keeps trying to get it,
or in other words, it *busy-waits*.

We don't need to change `spin_unlock()` to work with this implementation, or any 
subsequent implementation in this assignment.

### Testing your `spin_lock()` implementation

Open `tests.c` and take a look at the array `test_names`, which enumerates the
tests that are available, and the array `test_on`, which determines whether the
test at the same index in `test_names` will be run.  Right now, the test
harness is set up to only run the first test,
`"spin_try_lock_correctness_nograph"`.  The `"correctness"` part indicates that
it's a test that checks whether the `spin_try_lock()` implementation is
correct, not a benchmark that measures the performance of `spin_try_lock()`.

To test the correctness of `spin_lock()`, we'll need to change the `0` in `test_on`
corresponding to `spin_lock_correctness_nograph` to a `1`, rebuild the test
harness with `make`, and run it as before with `run`.  When your implementation
is working correctly, you should see a message like

```
-1: spin_lock_correctness_nograph succeeded for nt 24,
my_spinlock_shared_counter: 10000, sum: 10000, n_ops: 10000.  Values are
nonzero and the same, so the lock is working correctly.
```

Now the number of increments is equal to the number of operations attempted,
since each call to `spin_lock()` busy-waits until it has successfully acquired
`lock`, so workers successfully acquire the lock for each operation, and
are able to always increment the local and shared counters.

### Benchmarking your `spin_lock()` implementation

Now we know that `spin_lock()` is correct, but we don't know how efficient it is
compared to other forms of synchronization.  To get an idea of how we're doing,
we'll use the spinlock implementation from pthreads `pthread_spin_lock` as a baseline.

We need to turn on two benchmarking tests in `test_on` in `tests.c`: 
* `pthread_spin_lock`
* `spin_lock`

We aren't including a benchmark for `spin_try_lock()` here since it's an
apples-to-oranges comparison, as `spin_lock()` and `pthread_spin_lock()` both
busy-wait until a successful acquisition, and `spin_try_lock()` does not.

Now, rebuild the test harness and use `bench` to run benchmarks.

```bash
make
./bench 24 300000 1
```

You should see output that ends with some messages about generated graphs:

```
[1] "generating data/bench_t24_2018-01-09_12-27-07.csv_ticks_0accesses.pdf"
[1] "generating data/bench_t24_2018-01-09_12-27-07.csv_ticks_8accesses.pdf"
[1] "generating data/bench_t24_2018-01-09_12-27-07.csv_ticks_80accesses.pdf"
```

Check out the graphs; they should have one line for `spin_lock` and another for
`pthread_spin_lock`.  How do the implementations compare?

#### Bus contention

Note that 3 different graphs are generated, one labeled to represent test runs
with "no critical section accesses" `_0accesses.pdf`, another with "1 cache
line updated during critical section" `_8accesses.pdf`, and another with "10
cache lines updated during critical section" `_80accesses.pdf`.  The latter 2
tests simulate what threads usually do when they hold a lock, which is update
some other memory protected by the lock.  We simulate this by doing racy
increments to words in a shared circular buffer.  Because a given word in the
buffer is updated by multiple threads, increments to that word by different
threads require communication between those threads over the system bus.  This
is the case whenever multiple threads try to update the same word (among other
situations that trigger bus traffic). 

#### Questions
1. Does calling `spin_try_lock(&my_spinlock)` over and over again (even after
   we've just observed that `my_spinlock` is held) every time we need to get
   `my_spinlock` affect how long it takes to finish a set of operations?  Does
   each call require threads to communicate, and if so, what shared resources
   does that communication use?
2. How does this implementation scale as we increase the number of threads?
   Does passing the lock around and busy-waiting for it affect the time it
   takes for all threads to crunch through the set of operations?  If so, is the
   effect different for different numbers of threads?
3. As we add bus traffic, first by updating a single cache line and then by
   updating 10 cache lines while holding the lock, how do `spin_lock` and
   `pthread_spin_lock` respond to this change, and why?

#### A note on resource sharing, measurement accuracy, and interference

Since we'll all be using `babbage` to run these benchmarks, it's possible that another
student could be running benchmarks at the same time.  This will at the very least
make benchmarking take longer, and could make your graphs look a little strange.
To check whether this is going on, watch `htop` in another terminal to see if another
user is running a bunch of `./test` threads too.  If so, get in touch with that user
in one of the following ways to negotiate a way to share benchmarking time:
1. Via the `#cs510walpole` IRC channel on `irc.cat.pdx.edu`. 
   See the [CAT documentation](https://cat.pdx.edu/services/irc/) 
   for more information about getting connected and using IRC.
2. Using the `write` command to make a message appear in their terminal:
```bash
$ write theod
hello friend, will you be finished benchmarking soon?
(CTRL+d to end)
```
3. Getting in touch via email.  CAT and ODIN usernames are usually the same, so
   `username@pdx.edu` will probably work, but you could also use the mailing list
   to broadcast to everybody in the course (as a last resort) at
   [`cs510walpole@cecs.pdx.edu`](mailto:cs510walpole@cecs.pdx.edu).

We've tuned the `./run` and `./bench` invocations in this assignment to take
a few minutes or less, so the chance of collisions between users is relatively low.
Also, note that the correctness tests work no matter what, although they make take
longer if another user is running tests.

Finally, it's important to note that `babbage` is heavily used, and is a virtual
machine that shares a host with other heavily used virtual machines.  It's an
environment subject to frequent interference and weird behavior resulting from
virtualization and hyperthreading on the host, and benchmark results often
fluctuate.  Try running a given benchmark multiple times and looking at the
graphs that are produced to get an idea of the range of results, and if you
produce a particular graph with results that just don't make sense, try running
`./bench` again before you get too invested in being confused :)

### `spin_wait_lock()`

Based on our observations in the last section, it seems that spamming
`spin_try_lock()` as quickly as possible does not scale particularly well as we
increase the number of threads, especially in the presence of other bus
traffic.  Let's see what happens when we build a delay into our lock
acquisition function; we can write this modified version of `spin_lock()` in
`void spin_wait_lock(volatile uint64_t *lock)` in `worker.c`.  To be clear, the
only goal here is to poll the spinlock less frequently.  For example, it is
sufficient to just add a loop that, say, decrements an `int` 500 times, and
crank through the loop in between calls to `spin_try_lock()`.

Once you've implemented `spin_wait_lock()`, enable
`spin_wait_lock_correctness_nograph` and `spin_wait_lock` in `test_on` in
`tests.c`, `make` again, `run` as above until you've passed the correctness
tests, and then `bench` again as above and take a look at the generated graphs.
There should be a `spin_wait_lock` line now in addition to the lines from
previous graphs.

#### Questions
4. What result did you expect?  Are these graphs consistent with your expectation?
5. Does `spin_wait_lock()` result in more or less communication than
   `spin_lock()`?  Do the results shed any light on the relative merits of more
   and less communication?
6. Communication always comes at a cost, so we need to think carefully about how
   much bus traffic our implementations generate.  How can we minimize communication in
   our spinlock implementations?  Is there more than one way?
7. If we have just observed `*lock == LOCKED`, how likely is it that
   `spin_try_lock()` will succeed (returning `1`)?  Do you think that
   `spin_try_lock(lock)`, which can trigger an update, requires the same
   amount of communication as just loading the value of `my_spinlock`?  Why?  Do
   you think that these operations require unidirectional or bidirectional
   communication, and why?

### `spin_read_lock()`

Let's explore another way to minimize communication.  Instead of introducing a
delay while polling with `spin_try_lock()`, let's poll by *loading* `*lock` in
a tight loop until we observe `*lock == UNLOCKED` before each attempt at
`spin_try_lock()`.  Implement this approach in `void spin_read_lock(volatile
uint64_t *lock)` in `worker.c`.  We'd like to compare the effect of delaying
with the effect of polling with loads instead of `lockcmpxchgq()`, so don't
include the delay loop from `spin_wait_lock()` in this implementation.

Once you've implemented `spin_read_lock()`, enable
`spin_read_lock_correctness_nograph` and `spin_read_lock` in `test_on` in
`tests.c`, `make` again, `run` as above until you've passed the correctness
tests, and then `bench` again as above and take a look at the generated graphs.
There should be a `spin_read_lock` line now in addition to the lines from
previous graphs.

#### Questions
8. What result did you expect?  Are these graphs consistent with your expectation?
9. Does `spin_try_lock(lock)` always return `1` after we observe `*lock ==
   UNLOCKED`?  Why or why not?  If not, what does your implementation do in
   this situation?
10. Do the results suggest that one way of minimizing communication is better
    than the other?

### (Optional) `spin_experimental_lock()`

Finally, we offer a chance for intrepid souls to explore combining the
techniques we've tried above, and anything else you can imagine or read about
in the realm of spinlock implementation.  How close can you get to `pthread_spin_lock`?
How is `pthread_spin_lock` [implemented](https://sourceware.org/git/?p=glibc.git;a=blob;f=nptl/pthread_spin_lock.c;h=58c794b5da9aaeac9f0082ea6fdd6d10cabc2622;hb=HEAD) anyway? 
You may find the `xchgq()` primitive implemented in `util.h` helpful in this exploration.
Play and trial and error are the substance of experiential learning, so go crazy.

In keeping with previous steps, implement `spin_experimental_lock()` in `worker.c`,
and turn on `spin_experimental_lock_correctness_nograph` and `spin_experimental_lock` in
`test_on` in `tests.c`, then `make`, `run`, `bench` and repeat as usual.
