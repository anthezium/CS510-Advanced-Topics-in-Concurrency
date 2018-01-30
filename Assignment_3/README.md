# Assignment 3: Concurrent Queues

**Due Date: 4:19pm on Tuesday, February 6th**

## Overview

In this assignment, we're building our first concurrent data structures,
exploring the design space for concurrent queues.  In particular, we're
contrasting the performance scalability of a coarse-grained lock-based approach
with that of a (somewhat) fine-grained non-blocking approach.

The series of steps in this assignment build on one-another only conceptually,
so you can complete the implementations in any order to get things working
(just compare both of them once you're done).  However, the coarse-grained
lock-based queue depends on your solutions to Assignments 1 and 2.  There are
numbered questions throughout the assignment; we recommend thinking through
these questions carefully and writing down short concrete answers before moving
on to the next section.  If you're confused by a question or don't know where
to start, you're free to discuss or puzzle through them with other students,
and Ted and Jon are also available to help.  The point of these assignments is
to develop your understanding of the material and give you some hands-on
intuition for concurrent programming, and the questions are there to help you
understand what's going on.  If they don't, we'd really like to know, so we can
write some better ones :)

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
pushd Assignment_3
pushd data
# remove all data and graphs BESIDES the final ones you'd like me to look at
rm <out-of-date data and graphs>
popd
make clean
popd
# where NAME is your name separated by underscores
tar czf Assignment_3-NAME.tar.gz Assignment_2/
```
 
Email the archive to [theod@pdx.edu](mailto:theod@pdx.edu) with subject
`Assignment_3-NAME` and any questions and comments you'd like to share.
Include your answers to the numbered questions in the assignment in the text of
the email.

### Disjoint Access Parallelism

In previous assignments, we've explored a variety of ways to guarantee *mutual
exclusion*, that is, prevent two threads from trying to do something at the
same time.  Now that we're looking at data structures, we get a valuable
property (which will be a recurring theme in future readings and assignments)
from the fact that data structures have multiple separate pieces, which we may
be able to read and/or update separately *at the same time*.  Of course, this
introduces ample opportunities for weirdness, but it can also improve
throughput, since instead of threads waiting in line (or engaging in a
free-for-all) to access the data structure one-at-a-time, threads can get work
done in *disjoint* parts of the data structure simultaneously.

### The ABA Problem and Memory Reuse

We'll be representing a queue as a singly linked lists of nodes, where each node has a
`next` pointer to its successor,.  We'll be testing the queues with randomly
generated workloads that mix enqueues and dequeues roughly evenly, so it will
be convenient to be able to dynamically allocate and free memory as we enqueue
values (where we need a new node to hold the value) and dequeue values (where
the node that previously held the value is no longer needed).  Dynamic
allocation is only useful if we are able to reuse freed memory for subsequent
allocations (otherwise we might as well just statically allocate a large amount
of memory and fail when it runs out), so we've provided a memory allocator that
allows (limited) reuse.

One consequence of reusing memory is that a freed node and its new incarnation
have the same memory address.  Generally this doesn't matter, but for the
non-blocking queue, we'll be using `lockcmpxchgq()` to update `next` pointers.
Why does that make a difference?  Well, remember that `lockcmpxchgq(ptr, old,
new)` succeeds when `*ptr` has the value `old`, and we often use our last
observation of `*ptr` for `old` to implement the following:

> "If this memory location hasn't changed since the last time I looked at it,
> it's safe to make the change I was planning to make based on what I saw."

There's an assumption baked into this implementation: that if `*ptr` has the same
value it previously did, nothing has changed.  When we use `next` as `ptr`, this
amounts to assuming that nothing has changed if `next` still points to the same
chunk of memory.

Do you see how this could interact badly with reuse?  Broadly, the problem is
that as soon as we start reusing memory, we introduce the possibility that a
node could be freed, reallocated for a different purpose, and just happen to be
linked in at the same `next` pointer as it was in its previous life.  This
(admittedly unlikely) sequence of events could happen in between our initial
observation and our call to `lockcmpxchgq()`!  We'd have no way of knowing that
`next` was pointing to an entirely new node that happened to be using the same
memory.  Chaos!  The possibility that the world could change under you with you
none the wiser, merrily chugging along based on flawed assumptions, may be
enough to convince you that this scenario could cause problems.  If you'd like
a concrete example of such a problem, there is at least one lurking in this
assignment (in `nb_dequeue()`), which a question in that step will invite
you to identify and explain.  Stay tuned!

Well, what can we do about this?  There are a variety of solutions, some enumerated
in [Michael and Scott 1996](http://www.dtic.mil/get-tr-doc/pdf?AD=ADA309412).  You'll
be using a relatively simple one that is related to but subtly different from the
option Michael and Scott go with.  Basically, we observe that
1. We're allocating a cache line for each node, so node pointers will always be
aligned on a 64-byte boundary.  This means that the bottom 6 bits of these pointers
will always be 0s.  As long as we remember to clear those bits before dereferencing
a node pointer, we can keep some other stuff in those 6 bottom bits.
2. If we put something different in those bits every time we reuse the node, we'll
never have the "pointer" for successive incarnations, so `lockcmpxchgq()` will fail
when things have changed under us, so we avoid the ABA problem.
3. We can represent `2^6 = 64` distinct values in those 6 bits, so we can use each
cache line 64 times before resorting to other tricks to prevent the ABA problem.

This scheme is built into the memory allocator, which exposes two functions and
a macro for you to use:
1. `pool_allocate(d, &tagged_ptr)`, which allocates a cache line and stores its
   address and a "sequence number" (in the bottom 6 bits) in `tagged_ptr` (a
   `uint64_t`).
2. `untagged_ptr = (node_type*)get_qptr(tagged_ptr)`, a macro that zeroes out
   the bottom 6 bits in a tagged pointer to yield something you can dereference
   (after casting appropriately).
3. `pool_free(d, untagged_ptr)`, which marks the cache line at `untagged_ptr`
   as available for reuse.

To allocate a node and obtain a pointer we can dereference:
```c
uint64_t tagged_ptr;
pool_allocate(d, &tagged_ptr);
node_type *untagged_ptr = (node_type*)get_qptr(tagged_ptr);
// set a field
untagged_ptr->some_field = 5;
...
// to avoid the ABA problem, we need to use the _tagged_ version otherwise
lockcmpxchgq(&other_node_untagged_ptr->next, old, tagged_ptr);
```

To free a node:
```c
pool_free(d, (uint64_t*)untagged_ptr);
```

NOTE: `d` is just a pointer to data specific to the current thread, which includes
the metadata for memory allocation.

Note that the allocator requires a new fourth parameter `N_POOL_LINES` to
`bench` and `run`.
```bash
./bench MAX_THREADS N_OPS N_RUNS N_POOL_LINES
```

When you invoke `run`, you might see a message like this:
```
0: Ran out of memory in my allocation pool, crank up pool size or reduce number of operations.
```

If this happens, just increase the `N_POOL_LINES` parameter.  Try to do this
linearly, as there is a runtime cost linear in the pool size between tests.

All previously introduced primitives remain available (some will come in handy),
and are implemented and documented in `util.h`.

To keep the compiler from rearranging our code, e.g. during optimizations, we
_still_ need to use some other macros: If another thread might be loading from or
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

## Building Concurrent Queues

### `coarse_queue`

We'll start with a baseline concurrent queue that is just a sequential queue
protected by a lock you wrote of your choosing from Assignment 1 or 2.  Since
there is one big lock serializing all queue operaitons, we refer to this
approach as *coarse-grained locking*.

First, head over to `// TODO define types for coarse_node and coarse_queue` in
`tests.h` and familiarize yourself with the `coarse_node` and `coarse_queue`
types.  If you'd like to use a lock represented as something besides a
`uint64_t` (an `abql_nosharing` for example), change the type of the `lock`
field in `coarse_queue`.  If you make this change, you also need to head over
to `// TODO statically initialize global data for coarse_queue` in `tests.c`
and initialize your lock accordingly.  The final step for this change is
at `// TODO initialize lock state for each run` in `tests.c`, where you need
to reinitialize the lock.

For the functions below, note that `queue->head == queue->tail == 0` when the
queue is empty, and otherwise `queue->head` should point to the least recently
enqueued node and `queue->tail` should point to the most recently enqueued
node.  This means that `queue->head == queue->tail != 0` when there is one
element, and `queue->head != queue->tail` when there are multiple elements.

Later on, you may observe that this is slightly different from the situation
for `nb_queue`, which always keeps an extra "dummy node" in the queue.

#### `coarse_enqueue()`

Head over to `void coarse_enqueue(ti_data_in *d, volatile coarse_queue *queue,
uint64_t value)` in `worker.c`.  Allocate memory from the pool for the new node
with `pool_allocate()` and install `value` at `->val`. Acquire the lock in
`queue->lock` using your chosen implementation, link in a new tail and update
the `queue->head` and `queue->tail` pointers as needed, and then release the
lock.

This function should take constant time regardless of queue size, ignoring
synchronization overhead.

#### `coarse_dequeue()`

Head over to `int coarse_dequeue(ti_data_in *d, volatile coarse_queue *queue,
uint64_t *ret)` in `worker.c`.  Again, use `queue->lock` to protect your
critical section.  Therein, unlink the old head, store its `->val` in `*ret`,
and fix up the pointers in `queue`.  Thereafter, free the old head with
`pool_free()`.

If the queue is empty, this function should return `0`.  Otherwise, it should
return `1`.  This function should take constant time regardless of queue size,
ignoring synchronization overhead.

#### Testing your `coarse_queue` implementation

We're testing the following properties to determine whether our concurrent
queue implementations are correct:
1. If value A is enqueued before value B, then A is also dequeued before B (if
   dequeued).
2. No values are dropped, and no values are duplicated.  That is, the total
   number of values enqueued is equal to the total number of values dequeued
   plus the total number of values remaining in the queue at the end of the test.
If either check fails, the test harness terminates with an error condition and
message, as usual. 

`coarse_queue_correctness_nograph` tests your implementation for these
properties, and should already (and exclusively) be on.

```bash
./run 24 3000 10 36000
```

This `run` invocation should be good enough for testing correctness.

#### Questions
1. Can an enqueuer and dequeuer concurrently proceed in this implementation?  Why or why not?

### `nb_queue`

Now, let's implement the non-blocking concurrent queue from 
[Michael and Scott 1996](http://www.dtic.mil/get-tr-doc/pdf?AD=ADA309412) 
(see Figure 1).  Note that their `CAS()` corresponds to our `lockcmpxchgq()`,
`new_node()` to `pool_allocate()`, `free()` to `pool_free()`, and `*pvalue` to
`*ret`.

NOTE: Below, I've used a naming convention where a tagged pointer is stored in
a variable with a plain name, e.g. `head`, and the untagged version of the same
pointer is stored in a corresponding variable that ends in `_ptr`, e.g.
`head_ptr`.

First, head over to `// define types for nb_node and nb_queue` in `worker.c`
and familiarize yourself with these types.  Next, head over to `// statically
initialize global data for nb_queue` in `tests.c` and familiarize yourself with
the initial queue state (it is restored after each test at `// initialize
nb_queue state for each run`).

There is always a "dummy" node at the head of the queue in this implementation.
It is either the initial dummy we place below, or a subsequently dequeued node
pressed into dummy service.  Thus, when the queue is empty, `queue->head` and
`queue->tail` both point to the dummy (they are not `0`, as in `coarse_queue`),
and `dummy->next == 0` (that is, there isn't a half-complete concurrent enqueue
afoot).  When the queue is not empty, `queue->head` always points to the dummy
node, while `queue->tail` points to the actual tail (unless we're in the middle
of an enqueue operation, and `queue->tail` has temporarily fallen behind).

Next, head over to `// TODO allocate and initialize new dummy node for
nb_queue` in `worker.c`.  We need to set up the dummy node here before each
run: Allocate a line from the pool, set its `->val` to `1`, and point both
`my_nb_queue.head` and `my_nb_queue.tail` at it (its _tagged_ pointer, fresh 
from `pool_allocate()`).

#### `nb_enqueue()`

Head over to `void nb_enqueue(ti_data_in *d, volatile nb_queue *queue, uint64_t
value)` in `worker.c`.  

As before, we need to allocate a line for a new `nb_node` `node` and install
`value`.  Next, we need to repeat the following "linking" process until we
successfully link `node` in as the successor of the current `queue->tail`.  

NOTE: When I talk about "making an observation" below, that roughly translates
to invoking `CMM_LOAD_SHARED()`.

Basically, we make some observations, check that we are able to proceed while
these observations remain true, and then make whatever progress we can based on
the observations, using `lockcmpxchgq()` to verify that our observations remain
true when we're updating the queue as a consequence of them.  We make two kinds
of progress here: either pursuing our own objective by e.g. attempting to link
`node` into the queue, or advancing another thread's agenda by helping to
complete a multi-step operation the other thread is in the middle of, e.g.
"swinging" `queue->tail` to a node some other thread just linked into the
queue.

Specifically, we observe `queue->tail` and its `next` pointer (which should be
`0`, since it is the tail, after all).  Then we do a strange thing: we observe
`queue->tail` _again_ to see if it has changed.  Why do we do this weird thing?
Well, this algorithm (we'll see below) links the enqueued node into the queue
_before_ it updates `queue->tail` to point to it.  If a concurrent enqueuer has
just linked its own new node into the queue, we may observe this when we load
the `next` pointer above.  If we do, we know that a subsequent update to
`queue->tail` is coming.  In order to have any chance of observing this
subsequent update, we need to observe `queue->tail` after we observe the `next`
pointer.  We had to observe `queue->tail` first in order to observe its `next`
pointer, though, hence the two observations: the first just to get at `next`,
the second to have observed it after `next`.

If `queue->tail` has been concurrently updated, we need to start over again and
make new observations.  However, if it hasn't, we have a chance!  
* If, additionally, `next` was `0` when we observed it, that means that we
  observed the queue in a state where a concurrent enqueue was not between
  updates for the duration of both of our observations of `queue->tail`.  Maybe
  the queue is still in that state... let's try to update `next` (if it still
  hasn't changed) to install `node` as the tail's new `next` with
  `lockcmpxchgq()`.  If we get back the old `next`, we know that we successfully
  installed `node`, and have completed the linking process.
* If, on the other hand, the tail had a successor when we observed it, that
  means that we observed the queue in a state where a concurrent enqueuer had
  updated the current tail's `next` to point to a new tail, but not yet updated
  `queue->tail`.  That enqueuer may or may not have subsequently updated
  `queue->tail` to point to the new tail yet, though, so we might as well try to
  help out by using `lockcmpxchgq()` to update `queue->tail` (if it still has the
  value we observed) to point to the new tail.  At the very least, this means
  that we won't be stymied again by this particular half-formed operation after our
  next round of observations.  This is all we can do with this round of
  observations, so we need to start the linking process over again.

After the linking process, we still need to "swing" `queue->tail` to point to
the new tail.  We have successfully installed `node` as the new tail at this
point, so if `queue->tail` remains as it was when we observed it, we can safely
update it to point to `node`.  Whether this succeeds or fails, we're done; if
it succeeds, we did both updates for our enqueue ourselves, and if it fails, we
did the linking update, and somebody else beat us to the tail-swinging update.

#### `nb_dequeue()`

Head over to `int nb_dequeue(ti_data_in *d, volatile nb_queue *queue, uint64_t
*ret);` in `worker.c`.

The basic idea is the same here, and only `queue->head` is pointing to the node
we're dequeueing, so updating that successfully is sufficient to achieve our
objective.  Nevertheless, we are upright non-blocking citizens, committed to
always making progress, and will lend help to the tail-swinging cause if we
encounter it unswung in the course of fulfilling our duties.  We repeat the
following unlinking process.

Observe `queue->head`, then `queue->tail`, and then the head's `next` pointer.
Note that because `queue->head` always points to the dummy node, head's `next`
pointer (hereafter `next`) is either `0` (if the queue is empty) or points to
a node that contains the value we're trying to dequeue.

As before, we then observe `queue->head` again to detect whether our
observations are spread across a concurrent dequeue (i.e. `queue->head` has
changed since our initial observation), which would either update `queue->tail`
(did some helping) before it updates `queue->head`, or just update
`queue->head` (no opportunity for helping).

If our observations are not spread across a concurrent dequeue (that is,
`queue->head` has not changed), then we have two cases to consider:
* The queue is empty, or it was just empty, and there is an incomplete enqueue
  afoot which has yet to update `queue->tail`.  This is the case when our
  observations of `queue->head` and `queue->tail` are equal.  In this situation,
  both point to the dummy node, so the `next` pointer we observed was the dummy's
  `next` pointer.
  * If we observed the dummy's `next` pointer to equal `0`, then no concurrent
    enqueue was afoot, since it would have started by updating the dummy's
    `next` pointer to something besides `0`, and we observed `queue->tail` before
    we observed the dummy's `next` pointer, so we can't have observed the enqueue's
    update to `queue->tail` but missed its update to the dummy's `next` pointer.
    Since no concurrent enqueue was afoot, the queue remains empty, so we can't
    dequeue anything and must indicate this by returning `0`.
  * Otherwise, we observed the dummy's `next` pointer as nonzero (but also
    observed `queue->head` equal to `queue->tail`), so a concurrent enqueue
    must have updated the dummy's `next` pointer, but not yet swung `queue->tail`
    to match it.  In this case, we can use `lockcmpxchgq()` to update `queue->tail`
    (if it still matches our previous observation) to our observation of the
    dummy's `next` pointer.  If this succeeds, we have contributed to the greater
    good.  If not, that's fine, since it just means that another thread swung it
    first.  Either way, we still haven't dequeued anything yet (or observed the queue
    as empty), so we need to try the unlinking process again from the beginning with
    fresh observations.
* Otherwise, the queue has at least one node in it whose enqueue operation has
  completed.  In this case, we can actually dequeue something!  Recall that
  `queue->head` always points to the dummy node, so the value we're dequeuing
  here is `next_ptr->val`.  We need to store this value to `*ret` before the next
  step, more on that in a second.  Next, we need to retire the old dummy (our
  observation of `queue->head`) and install the node we're dequeueing (`next`) as
  the new dummy.  To avoid duplicate dequeues, we need to make sure that no
  concurrent dequeue has beat us to this, so we need to use `lockcmpxchgq()` to
  swap in `next` at `&queue->head`, provided it has not changed since our last
  observation.  If this succeeds, we have dequeued node whose valued we copied to
  `*ret`, and it is the new dummy.  If `lockcmpxchgq()` fails, then a concurrent
  dequeuer (enqueuers don't update `queue->head`) beat us to dequeueing this
  node, and try the unlinking process again with fresh observations.
  
Once we've succeeded in dequeueing a node, the old dummy we unlinked is no
longer needed.  We can safely free it with `pool_free()`.  This is why we
needed to copy the value to `*ret` above before installing its corresponding
node as the new dummy: as soon as it becomes the new dummy, a concurrent
dequeuer could unlink and free it, which would be a real bummer if we hadn't
copied the value it contained yet.

Our dequeue succeeded, so we can proudly return `1`.

#### Testing your `nb_queue` implementation

Flip on `nb_queue_correctness_nograph` in `tests_on` in `tests.c`, and use
`run` as before to check this implementation against the same criteria as
`coarse_queue`.

#### Benchmarking your queue implementations

There are two groups of benchmarking tests.
* `coarse_queue` and `nb_queue` both run a workload of approximately half
  enqueues and half dequeues in a random order (different for each run).
* `coarse_queue_enq` and `nb_queue_enq` both run a workload of entirely
  enqueues.

Turn all of these tests on in `tests_on` in `tests.c`, turn the correctness
tests off (to save time) and generate a cool graph:

```bash
./bench 24 3000 1000 36000
```

It will take a bit longer than benchmarking runs for previous assignments.
Since we aren't measuring locks anymore, the critical section accesses (which
are there to simulate data updates inside critical sections) are turned off
for these tests.  That means that we'll only be generating `00accesses` graphs,
not the other two kinds we usually see.

#### Questions
2. Can an enqueuer and dequeuer concurrently proceed in this implementation?
3. How does `nb_queue` compare to `coarse_queue` (i.e. for a mixed workload)?
4. How does `nb_queue_enq` compare to `coarse_queue_enq` (i.e. for a workload
   of only enqueues)? 
5. What primitives does `nb_enqueue()` use to make updates on a best-case
   enqueue?  What about `coarse_enqueue()`?
6. Do the implementations compare in the same way for both kinds of workloads?
   Why or why not?
7. Where would the ABA problem break this operation?  What could go wrong as a
   result?

Copyright Ted Cooper, January 2018
