# Assignment 2: FIFO Spinlocks

**Due Date: 4:19pm on Tuesday, January 23rd**

## Overview

In this assignment, we're exploring the design space for spinlocks that provide
a fairness property (in addition to solving the critical section problem):
If thread A's acquisition attempt takes effect before thread B's acquisition attempt,
thread A will get the lock before thread B.  In all the implementations we'll explore,
this is achieved by tying the order in which threads acquire the lock to the coherence
order for a word in memory.

The series of steps in this assignment build on one-another only conceptually,
so you can complete the implementations in any order to get things working (just
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
[here](https://lwn.net/Articles/267968/):

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

We'll need to use the `old = xchg(ptr, new)` primitive, defined in `util.h`.
This primitive is an atomic unconditional swap, that is, it atomically swaps
the current value in `*ptr` (returned as `old`) for `new`.

To keep the compiler from rearranging our code, e.g. during optimizations, we
need to use some other macros: If another thread might be loading from or
storing to `x`, write `_CMM_STORE_SHARED(x,v);` instead of `x = v;`, and
`CMM_LOAD_SHARED(x)` instead of `x` whenever we load `x`.  For example, if we
were looping on the value of `x`, we would write `while(CMM_LOAD_SHARED(x)) {
... }` instead of `while(x) { ... }`, and if we were loading `x` into a local
variable `y`, we would write `y = CMM_LOAD_SHARED(x)`.

Note that storing an aligned `uint64_t` with `_CMM_STORE_SHARED()` is atomic on
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
* `next`, which is the next number that will be handed out to somebody in line
* `owner`, which is the number of the ticket that belongs to the current lock
  holder.

Head over to `// define a type for ticket` in `tests.h`, and familiarize
yourself with the type `ticket_state` and the global declaration of
`my_ticket_lock`.  As you implement the operations below, consider whether
`next` and `owner` should be on the same cache line or not. 

Next, head over to `ticket_lock` in `worker.c`.  To acquire the lock, you
atomically increment `next`, grabbing its previous value as your number, and
then spin until another thread updates `owner` to match your number (if there's
nobody ahead of you in line, this will already be the case).  Now it's your
turn to execute your critical section!

Finally, head over to `ticket_unlock` in `worker.c`.  To release the lock, you
simply increment `owner`, to let your successor know that it's their turn.
Note that only the lock holder updates `owner`, so no other thread will race
you here.

#### Testing your `ticket_lock()` and `ticket_unlock()` implementations

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
performance of this implementation to some of your unfair spinlocks from the
last assignment.  In particular, `spin_lock` and `spin_read_lock` (or
`spin_experimental_lock`) (just paste them into `worker.c` from your previous
submission) establish helpful reference points, and `pthread_spin_lock` is
still a good baseline.

We've observed slightly smoother data with a smaller set of operations and
multiple runs, like so

```bash
./bench 24 3000 80
```
#### Questions

1. How does `ticket_lock` compare to its unfair counterparts?
2. In particular, why is there a gap between `spin_lock` and `ticket_lock`?
3. What memory location(s) are threads contending to update in this implementation?
4. What memory location(s) are threads spinning on in this implementation?
5. What communication (to whom) is triggered when `ticket_unlock` writes to `owner`?

### `abql_(no)sharing_lock()` and `abql_(no)sharing_unlock()`

Let's explore a queueing lock that works roughly the same way as `ticket_lock`
on the acquisition side, but spreads out spinning and successor notifications
to per-thread words in memory on the release side, rather than the single word
`owner` used by all waiting threads in `ticket_lock`.  You've read about this
Array-Based Queueing Lock (AQBL) in 
[Anderson 1990](http://www.cs.pdx.edu/~walpole/class/cs510/papers/02.pdf).
See Table V for Anderson's ABQL implementation.  NOTE: Anderson's
`ReadAndIncrement` corresponds to our `lockxaddq`.

#### Preparing data structures

Head over to `// define a type for abql_sharing` in `tests.h` and familiarize
yourself with the `flag_sharing` type and the global declaration of `flags_sharing`.

Next, head over to `// TODO declare and initialize data for abql_sharing` in
`tests.c` and declare and initialize an array of `n_threads` `flag_sharing`s,
where the first element has `val` set to `HAS_LOCK` and each subsequent element
has `val` set to `MUST_WAIT`.  Point `flags_sharing` to this array.  Now
`flags_sharing` will be supplied as `flags` to each call to
`abql_sharing_(un)lock()`.  Additionally, note that `queue_last_sharing` is
declared in `tests.h`, initialized to `0` in `tests.c`, and will be supplied as
`queue_last` to each call. 

#### `abql_sharing_lock()`

Head over to `abql_sharing_lock()` in `worker.c`.  Instead of atomically
incrementing `next` and then spinning on `owner` (along with all other threads
waiting for the lock) as in `ticket_lock`, this lock acquisition operation will
atomically increment `queue_last`, and use the value handed back from the
atomic increment (the previous value of `queue_last`, uniquely returned to this
thread) as an index into `flags`, indicating which element in the array this
thread should spin on.  This way, each thread in the queue ends up spinning on
a different `flags[i].val`, so updates to e.g. your predecessor's
`flags[j].val` won't require your cached copy of `flags[i].val` to be
invalidated (which takes time).  Right?  When you index into `flags`, though,
note that `queue_last` just keeps growing as this algorithm proceeds,
while there are only `n_threads` elements in `flags`, so you'll need to wrap
around.

Store this index in `*my_place`, because we'll need it to persist from this
call to `abql_sharing_lock()` to the corresponding call to
`abql_sharing_unlock()`. Next, wait for your flag to no longer be `MUST_WAIT`.
When this happens, it's because your predecessor in the queue is letting you
know that it's your turn, so you can just set the flag back to `MUST_WAIT` for
the next thread who ends up here, and proceed on your merry way.

#### `abql_sharing_unlock()`

Head over to `abql_sharing_unlock()`.  We still have `*my_place` from the call
to `abql_sharing_lock()`, so we know where our flag is.  The thread who entered
the queue after us got back the next value of `queue_last` as its... place, and
that is just `*my_place + 1` (we're atomically _incrementing_), so we know
where our successor's flag is, too.  All that remains, then, is to tap our
successor on the shoulder and let them know it's their turn by writing
`HAS_LOCK` to their flag. 

#### Checking for (partial) correctness

Before implementing the `nosharing` verions, it will simplify things to check
`abql_sharing_(un)lock()` by turning on `abql_sharing_correctness_nograph` in
`tests.c` and verifying that your implementations correctly solve the critical
section problem.

#### `abql_nosharing_(un)lock()`

Head over to `// TODO define a type for abql_nosharing` in `tests.h` and
define a type `flag_nosharing` that still contains a single `uint64_t` `val`,
but also contains enough padding to occupy an entire cache line (8 words or 64
bytes in a cache line).

Next, head over to `// TODO declare and initialize data for abql_nosharing` in
`tests.c` and declare and initialize an array of `n_threads` `flag_nosharing`s
(which is itself aligned on a cache line boundary), where the elements are
initialized as before.  Point `flags_nosharing` to this array.  

`nosharing` versions of all our familiar variables will be passed to
`abql_nosharing_(un)lock()` in the same way, which is convenient, because it
means we can just copy the text of the `sharing` version of each operation and
replace `sharing` with `nosharing`.

#### Testing your implementations

Turn on `abql_nosharing_correctness_nograph`, `abql_sharing_lock`,
`abql_nosharing_lock` (and leave `ticket_lock` on) and `bench` as before.

#### Questions
6.  How does `abql_sharing_lock` compare to `abql_nosharing_lock`?  Why?
7.  What memory location(s) are threads contending to update in this implementation?
8.  What memory location(s) are threads spinning on in this implementation?
9.  What communication (to whom) is triggered when `abql_nosharing_unlock`
    writes to `flags[successor_index].val`?  How does this compare
    to the communication triggered by `ticket_unlock`?
10. How does `abql_nosharing_lock` compare to `ticket_lock` and why?

### (Optional) `mcs_(no)sharing_lock()` and `mcs_(no)sharing_unlock()`

Another queueing lock is the MCS lock, described accessibly [on LWN](https://lwn.net/Articles/590243/) and originally introduced in
[Mellor-Crummey and Scott 1991](https://cis.temple.edu/~qzeng/cis5512-fall2015/papers/p21-mellor-crummey.pdf) 
(see Figures 5 and 7).  NOTE: This paper uses alternate names for our
primitives.  `fetch_and_store` corresponds to `xchgq`, while `compare_and_swap`
corresponds to `lockcmpxchgq`.

The MCS lock, named after its inventors, is basically the linked list to ABQL's
array.  To hand off the lock, the lock holder still notifies its successor that
the successor can proceed, much like how the lock holder updates its
successor's `flags_(no)sharing` value in `abql_(no)sharing_release()`.  Instead
of an array of flags, we have a local pair of values `next` and `locked` for
each thread (`local_lock`), plus another global pair that new threads use to
join the queue (`global_lock`).

If `global_lock->next` is null, there is no queue (and so no tail of the
queue), and the lock is not held.  This is initially the case (otherwise,
nobody could initially acquire it), see the initializations of
`mcs_global_(no)sharing` in tests.c.  Otherwise, `global_lock->next` points to
the tail of the queue of local locks, each corresponding to a thread waiting in
line to acquire the lock (besides the head, which corresponds to the lock
holder).

The holder's local lock is at the head of the queue, with its
`local_lock->next` pointing to the holder's successor's lock, starting a chain
that ends at the queue tail.  If `local_lock->next == local_lock` or
`local_lock->next` is null, then the holder has no successor, and is alone in
the queue (at least until one comes along, which could happen at any time).

If `CMM_LOAD_SHARED(local_lock[i]->locked) == LOCKED`, then thread `i` is
waiting for its predecessor to update `local_lock[i]->locked` to `UNLOCKED`.
This is how the predecessor hands off the lock to thread `i`.  Otherwise,
`local_lock[i]-> LOCKED` is always `UNLOCKED`.

#### Preparing data structures

To implement this, it will help to define a type for these locks.  For now, we
won't worry about false sharing, and just use the definition for the type
`mcs_sharing` in `tests.h` where there it says `// define a type for
mcs_sharing`.  Giving its members type `uint64_t` will necessitate some casting
back and forth between `mcs_sharing*` and `uint64_t` in the implementation, but
makes it easier to work with our primitive operations, which expect
`uint64_t`s.  Also note the global declarations of `mcs_global_sharing` and 
`mcss_sharing` below.

Next, head over to `// TODO declare and initialize data for mcs_sharing`
in `tests.c` and declare and initialize an array of `n_threads` `mcs_sharing`s,
each with `next` set to `0` and `locked` set to `UNLOCKED`.  Point
`mcss_sharing` to this array.  Now `&mcss_sharing[i]` will be supplied as
`local_lock` to each call to `mcs_sharing_(un)lock()`, where `i` is the thread
number (that is, the `i` field in `ti_data_in`) of the caller.  `global_lock`
will contain another instance of `mcs_sharing`, initialized in the same way,
indicating that the lock is initially not held.

#### `mcs_sharing_lock()`

Now we're ready to implement the operations for this lock.  Head over to the
definition of `mcs_sharing_lock()` in `worker.c`.  To acquire the lock, clear
your `local_lock->next`, then `xchgq` `global_lock->next` for your `local_lock`
to install your lock as the new tail of the queue.

If it comes back that there was no old tail, you're the only thread in the
queue right now, and you're done.  Otherwise, you need to put your lock after
the old tail in the queue.  Since you'll be waiting for the old tail to finish,
mark `local_lock->locked` as `LOCKED`, and then let the thread corresponding to
the old tail know that you're its successor by updating the old tail's `next`
field to point to your `local_lock`.  Now that your lock is linked into the
queue, all you need to do is wait for your predecessor to give you the go-ahead
by updating `local_lock->locked` to `UNLOCKED`.  Once that happens, you've
successfully acquired the lock.

#### `mcs_sharing_unlock()`

Head over to `mcs_sharing_unlock()` in `worker.c`.  To release the lock, first
check whether you have a successor (that is, whether `local_lock->next` is
non-null).

If you have a successor, all you need to do is update your successor's lock's
`locked` field to `UNLOCKED`.  Now you've handed the lock off to your
successor, and are done.

If you don't have a successor, things are little more complicated.  You've
observed no successor, so `global_lock->next` _should_ already be pointing to
your `local_lock` since there is nobody else in the queue, but you need to
allow for the possibility that other threads snuck in and added themselves to
the queue after you observed `local_lock->next` to be null.  You can use
`lockcmpxchgq()` here to update `global_lock->next` to `0` if it still holds
`local_lock` (nobody snuck in).  In this case, you've removed yourself as the
last remaining queue member, the lock is no longer held, and you're done.  If
`global_lock->next` now points to a different lock (that is, `lockcmpxchgq()`
gave back a value `!= local_lock`), then some other thread has concurrently
added itself to the queue, and thinks you're its predecessor.  This other
thread will eventually update your `local_lock->next` to point to its own lock
(i.e. get behind you in line), so you need to wait for it to do that (to find
out who the successor is), then update the successor's lock's `locked` field to
`UNLOCKED`.  Now you've handed the lock off to your successor, and are done.

#### `mcs_nosharing_(un)lock()`

Let's see what the impact of false sharing is on MCS performance.  Fill in 
`// TODO define a type for mcs_nosharing` in `tests.h` with a version that is
padded out fill a cache line (64 bytes, or 8 `uint64_t`s).

Fill in `// TODO declare and initialize data for mcs_nosharing` in
`tests.c` with code that initializes and assigns to `mcss_nosharing` as before,
but guarantees that the array `mcss_nosharing` is aligned on cache line
boundaries (you can always declare a variable with
`__attribute__((aligned (64)))`, for instance).

Head over to `worker.c`, copy your implementations of `mcs_sharing_lock()` and
`mcs_sharing_unlock()` and replace `sharing` with `nosharing` within the copies
to obtain your `mcs_nosharing_(un)lock()` implementations.  Wouldn't
language-level support for specializing polymorphic code to multiple concrete
types come in handy about now?

### Alternative `mcs_(no)sharing_unlock()`

BONUS: Mellor-Crummey and Scott propose an alternative `unlock` implementation that uses
an unconditional exchange operation (such as `xchgq`) instead of
`lockcmpxchgq`, in Figure 7 of
[Mellor-Crummey and Scott 1991](https://cis.temple.edu/~qzeng/cis5512-fall2015/papers/p21-mellor-crummey.pdf) 
and provide a detailed explanation (including an example) in the latter half of
Section 2.4.  Implement this variant and compare your graphs.

#### Testing your implementations

Turn on `mcs_(no)sharing_correctness_test` and `mcs_(no)sharing_lock` and
rebuild to check and benchmark your implementations as usual.

#### Questions
11. How do the `sharing` and `nosharing` variants of MCS compare?  Do they
    differ as much as `abql_sharing_lock` and `abql_nosharing_lock`?  Why?
12. How does MCS compare to ABQL?  Does one consistently beat the other?  If not,
    when does MCS win?  When does ABQL win?  Why?
13. Compare Figures 15 through 17 with Figure 18 in
    [Mellor-Crummey and Scott 1991](https://cis.temple.edu/~qzeng/cis5512-fall2015/papers/p21-mellor-crummey.pdf).
    Do `mcs` and `anderson` (ABQL), compare in these figures the same way that
    MCS and ABQL compare in your graphs?
14. Why do you think Mellor-Crummey and Scott included 3 graphs for tests with
    "empty" critical sections and only 1 with a nonempty ("small") critical
    section?
15. BONUS: How do the `lockcmpxchgq`-based and alternative `unlock`
    implementations' performance compare?  Why?
16. BONUS: Explain how the alternative `unlock` implementation breaks fairness.

Copyright Ted Cooper, January 2018
