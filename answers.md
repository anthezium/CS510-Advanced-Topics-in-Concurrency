`Assignment_1`:
1. **Does calling `spin_try_lock(&my_spinlock)` over and over again (even after
   we've just observed that `my_spinlock` is held) every time we need to get
   `my_spinlock` affect how long it takes to finish a set of operations?  Does
   each call require threads to communicate, and if so, what shared resources
   does that communication use?**
Yes, it consistently increases the total time it takes to complete the set of
operations as we increase the number of threads.  All of the lock
implementations we explore in this assignment have this proprety, but the
amount of time added as threads are added is greatest for this approach.
Because `lockcmpxchgq()` is an update it requires us to invalidate copies of
the cache line that holds the lock in other cores' caches and wait for their
invalidation acknowledgements to come back, and also either delay responding to
their racing invalidations or fight with them until we own the cache line long
enough to successfully update it without it changing under us.  This
communication all occurs over the shared interconnect between cores,
historically a bus.  Regardless of how the interconnect is structured,
bandwidth is limited, and is shared with any other non-local memory accesses
being serviced on the system.
2. **How does this implementation scale as we increase the number of threads?
   Does passing the lock around and busy-waiting for it affect the time it
   takes for all threads to crunch through the set of operations?  If so, is the
   effect different for different numbers of threads?**
   Yes, as noted above it takes longer to do the same number of acquisition
   release cycles as we add threads.
   
   Here's a probabilistic argument:
    1. A critical section that has no work still involves one memory access,
       namely the store of the unlocked value to the lock when it completes.
This can be a cache hit or a cache miss depending on whether other threads have
attempted to acquire the lock.
    2. The probability of other threads attempting to acquire the lock before
       the lock holder releases the lock increases as the number of contending
threads grows. This will contribute to an increase in time taken to execute the
critical section (where that includes the time to actually release the lock).
    3. A cache miss when attempting to release the lock must not only wait for
       the time taken to communicate the lock value from wherever it is, to the
local cache, and to invalidate any shared copies of it and get it in exclusive
mode.  It also does not take effect from other threads' perspectives until the
write is propagated to another core or main memory.  The amount of time this
write must wait to be published over the interconnect depends on the level of
contention from other cores.  The more threads we have executing compare
exchanges on the lock, the more interconnect contention we will have, and the
longer the delay before sending the above messages will tend to be. This
contributes to the time before another thread can begin its critical section.

3. **As we add bus traffic, first by updating a single cache line and then by
   updating 10 cache lines while holding the lock, how do `spin_lock` and
   `pthread_spin_lock` respond to this change, and why?**
As noted in the previous answer, the more interconnect traffic we have, the
longer it will take to acquire and release the lock, since both operations
require communications and therefore interconnect bandwidth.
4. **What result did you expect?  Are these graphs consistent with your expectation?**
Not really relevant to solutions.  I was disappointed that none of them beat
`pthread_spin_lock`.
5. **Does `spin_wait_lock()` result in more or less communication than
   `spin_lock()`?  Do the results shed any light on the relative merits of more
   and less communication?**
`spin_wait_lock()` attempts to acquire the lock less frequently, and doesn't do
anything else that requires communication in the interval between acquisition attempts,
so it results in less communication.  We can see from the fact that it beats `spin_lock()`
that the added communication `spin_lock()` generates actually slows things down overall.
6. **Communication always comes at a cost, so we need to think carefully about how
   much bus traffic our implementations generate.  How can we minimize communication in
   our spinlock implementations?  Is there more than one way?**
There are many ways.  Two that we explore in this assignment are delaying
instead of polling as quickly as possible (no communication during the delay),
and polling with loads rather than `lockcmpxchgq()` (no additional
communication as long as you hold the cache line with the lock in shared mode).
There are plenty of different ways to delay, one that we don't explore in this
assignment (but can be effective in practice) is exponential backoff.
7. **If we have just observed `*lock == LOCKED`, how likely is it that
   `spin_try_lock()` will succeed (returning `1`)?  Do you think that
   `spin_try_lock(lock)`, which can trigger an update, requires the same
   amount of communication as just loading the value of `my_spinlock`?  Why?  Do
   you think that these operations require unidirectional or bidirectional
   communication, and why?**
    * It's not very likely `spin_try_lock()` will succeed, since it would require
that the lock holder release the lock in between our observation of `*lock` and
subsequent call to `spin_try_lock()`, which is (presumably) not a lot of time.

    * Two cases:
        1. Loading: Initially, we need to send out a read request and receive a
           response to load `my_spinlock` (a round trip).  However, we also
might be able to snoop the value when it is sent in response to another core's
read response or flushed to memory, in which case we receive the value without
ever sending out a read request (1 message). Then, as long as we hold it in
shared mode (i.e. we haven't received an invalidation from a concurrent
update), we'd trigger no more communication.
        2. `spin_try_lock(lock)`: This is assuming a
           "load-linked/store-conditional"-style implementation of compare and
exchange that retries until it succeeds.  I have no idea what Intel actually
does.  We'd (at a minimum) send out a read for ownership, receive an
invalidation acknowledgement from the previous owner (assuming nobody had it in
shared mode), succeed with the update while we still own the line with the
lock, then send out the new value to whoever asks for it next.  This doesn't
sound too bad (`3` messages end to end), but what happens if some other
core reads for ownership while we have the line in exclusive mode but before
we've completed the update?  Then our update fails, and we'll have to retry.
That's another invalidation round trip just for the other core to get the line
in exclusive mode, and another for us to get it back (whether the other core's
update has succeeded or not), and we could fail again.  So, in the best case
this is more communication, and in the worst case, it's a lot more
communication.  We could imagine an alternative implementation, where we don't
acknowledge other cores' invalidations on lines we're updating atomically, but
this is still expensive, since they can't acquire ownership until they hear
back from us.  Note: This section is badly in need to advice from somebody who
knows more about the implementation of atomic instructions.  I'll attempt to
get in touch with such a person and update it.

8. **What result did you expect?  Are these graphs consistent with your expectation?**
One might expect spinning on reads to be more effective than just waiting
between `spin_try_lock()` attempts, but they are a wash with no extra
interconnect traffic, and `spin_wait_lock()` performs better as we add traffic.
While it's likely that a message ends up being sent to each core spinning on
read every time the lock is updated, cores that are delaying will neither send
nor receive any messages (besides responding to invalidations if we have an
invalidation queue) during the delay, even if the lock is updated.  This
reduces the total number of messages, but also means that a core in the middle
of a delay won't always notice that the lock has been released and proceed as
soon as it otherwise would.  Thus, both techniques have advantages and
disadvantages.  Perhaps we could get the best of both worlds by combining them.
9. **Does `spin_try_lock(lock)` always return `1` after we observe `*lock ==
   UNLOCKED`?  Why or why not?  If not, what does your implementation do in
   this situation?**
No, because some other thread could have concurrently acquired the lock in
between our observation and call to `spin_try_lock(lock)`!  My implementation
retries in this case, spinning on read again.
10. **Do the results suggest that one way of minimizing communication is better
    than the other?**
See the answer to 8.

`Assignment_2`:
1.  **How does `ticket_lock()` compare to its unfair counterparts?**
2.  **In particular, why is there a gap between `spin_lock()` and `ticket_lock()`?**
In `spin_lock()`, our call to `lockcmpxchgq()` may not return `UNLOCKED`.  In
this case, we call `lockcmpxchgq()` again (and keep going) until we get back
`UNLOCKED`.  This means that each call to `spin_lock()` can result in many
calls to `lockcmpxchgq()`, generating substantial bus contention by repeatedly
invoking an atomic update instruction.  `ticket_lock()`, on the other hand,
just calls `lockxaddq()` once on `lock->next`, and uses the return value as the
caller's ticket number regardless of what it is.  This means that each call to
`ticket_lock()` results in exactly one atomic update, instead of potentially
many.  After that, all it does is spin on reading `lock->owener`, which only
results in bus traffic when `lock->owner` is updated by a thread releasing the
lock.  Thus, `ticket_lock()` generates substantially less bus traffic on
average.
3.  **What memory location(s) are threads contending to update in this implementation?**
Two locations, both members of a single `ticket_state` struct shared by all
threads, see the declaration of `my_ticket_lock` in `tests.h` and initialized
in `tests.c`):
    1. `next`, which threads acquiring the lock contend to `lockxaddq()` on.
    2. `owner`, which the lock holder stores to upon lock release.
4.  **What memory location(s) are threads spinning on in this implementation?**
Just one location in the same shared struct as above, namely `owner`, which
threads waiting in line to acquire the lock spin on reading, each until its
number comes up as a result of the thread's predecessor updating `owner` to
release the lock.
5.  **What communication (to whom) is triggered when `ticket_unlock()` writes to `owner`?**
If no other threads are waiting for the lock, the core where the lock holder is
running may hold `owner`'s cache line in exclusive mode, in which case it's
possible that no communication will occur.  Otherwise (if there are threads
waiting to acquire the lock), then the cache line containing `owner` (hereafter
just `owner`) is in shared mode on all cores waiting for the lock (and the lock
holder core), and at least the following communication occurs: 
    1. The lock holder core sends invalidations for `owner` to all waiting cores.
    2. Waiting cores send back invalidation acknowledgements to the lock holder core,
which is waiting for these to come back before it can proceed.
    3. Having acquired `owner` in exclusive mode, the lock holder core updates it,
transitioning its copy of the line to modified mode.
    4. Some waiting core sends a read request for `owner`, which the lock holder
receives and responds to by flushing `owner` and sending its new value as a response to
the request and transitioning its copy of `owner` to shared mode.
6.  **How does `abql_sharing_lock()` compare to `abql_nosharing_lock()`?  Why?**
The `nosharing` version performs moderately better.  The `flag_sharing` struct
occupies a single word, so 8 `flag_sharing`s are fit into a single cache line
in the `sharing` version.  On the other hand, we have padded out the
`flag_nosharing` struct to occupy an entire cache line and aligned the array of
flags on a cache line boundary, so each flag occupies its own cache line.  This
means that when a lock holder releases a lock by updating its successor's flag,
in the `nosharing` version this results in the lock holder core sending an
invalidation for the cache line occupied by that flag to just 1 core (the
successor core) and waiting for just 1 invalidation acknowledgment from that
core before completing its update.  In the `sharing` version on the other hand,
up to 8 cores could have the cache line containing the successor's flag (and 7
more flags other cores could be spinning on) in shared mode, requiring 8
invalidations and 8 responses before the lock holder can complete its update.
7.  **What memory location(s) are threads contending to update in this
    implementation?**
`queue_last` (shared by all threads), the lock holder's currently assigned flag
(to set it back to `MUST_WAIT` after acquisition), and the successor's
currently assigned flag (to set it to `HAS_LOCK` upon release).
8.  **What memory location(s) are threads spinning on in this implementation?**
The thread's currently assigned flag (upon acquisition).
9.  **What communication (to whom) is triggered when `abql_nosharing_unlock()`
    writes to `flags[successor_index].val`?  How does this compare
    to the communication triggered by `ticket_unlock()`?**
Provided there is a successor waiting, then the holder:
    1. sends an invalidation to the successor, 
    2. waits for an receives an invalidation acknowledgement from the successor, and 
    3. sends a read response to the successor.
10. **How does `abql_nosharing_lock` compare to `ticket_lock` and why?**
`abql_nosharing_lock` eventually beats `ticket_lock` as we increase the number
of cores for all levels of bus traffic.  The less bus traffic we have, the
fewer cores it takes for `abql_nosharing_lock` to win.  We observe this trend
because while both locks use `lockxaddq()` on a single memory location to
initiate lock acquisition, upon release `tick_unlock()` writes to a memory
location that all other enqueued cores are spinning on (in shared mode),
requiring invalidations and acknowledgements linear in the number of enqueued
threads, while `abql_nosharing_unlock()` writes to a memory location (isolated
on its own cache line) that at most one other core is spinning on, requiring
only one invalidation and acknowledgement.  As the number of cores increases,
the difference in unlocking cost becomes more pronounced.  Other bus traffic
fuzzes out this distinction, since it results in a smaller proportion of the
total bus traffic (and thus overall time to complete a test) being attributable
to unlocking overhead.
11. **How do the `sharing` and `nosharing` variants of MCS compare?  Do they
    differ as much as `abql_sharing_lock()` and `abql_nosharing_lock()`?  Why?** 
`nosharing` still beats `sharing` for MCS, but usually (the difference is less
noticeable with an empty critical section) by only about half as large a margin
as that by which `abql_nosharing` beats `abql_sharing`.  This is because the
`flag_sharing` used in ABQL is 1 word, while the `mcs_sharing` used in MCS is 2
words, so 8 `flag_sharing`s can be falsely shared on a cache line, while
only 4 `mcs_sharing`s can be falsely shared on a cache line.
12. **How does MCS compare to ABQL?  Does one consistently beat the other?  If not,
    when does MCS win?  When does ABQL win?  Why?**
See the answer to 13.
13. **Compare Figures 15 through 17 with Figure 18 in
    [Mellor-Crummey and Scott 1991](https://cis.temple.edu/~qzeng/cis5512-fall2015/papers/p21-mellor-crummey.pdf).
    Do `mcs` and `anderson` (ABQL), compare in these figures the same way that
    MCS and ABQL compare in your graphs?**
This result (MCS beating ABQL for empty critical sections) is barely
reproducible in my tests, but this is likely a consequence of how the
experiment is set up (graphs from 3-hours runs using my reference solutions
attached).  

In the case where a thread observes the lock free when it attempts to acquire
it, and observes no successor when it attempts to release it, the same two
cache lines (`local_lock->next` and `global_lock->next`) updated in
`mcs_nosharing_lock()` are read and updated (respectively) in
`mcs_nosharing_unlock()`.  Although there are 3 cache lines updated here total,
2 of the updates are to the same cache line which (in the absence of concurrent
acquisitions) will remain in modified mode in the lock holder's cache, so from
the perspective of other cores, there are just 2 updates total in this case
(the first update to `global_lock->next` was not observed by anyone else).

On the other hand, `abql_nosharing_lock()` always updates `queue_last` and
the lock holder's flag, and `abql_nosharing_unlock()` always updates 
the successor's flag (even if no successor has jumped in line yet), resulting
in 3 total updates, all to different cache lines.  Thus, in the situation
outlined above, where there is no contention for the lock for the duration
of acquisition, critical section, and release, `abql_nosharing` does 1 more
cache line update than `mcs_nosharing`.

This should translate into a slight advantage for `mcs_nosharing` when
contention is extremely low.  We can arguably see this in `mcs_nosharing`
beating `abql_nosharing` for 1 thread (there will never be a predecessor or a
successor in this case), and probably don't reliably observe this in the data
for more cores because the experiment doesn't do any non-critical-section work,
so as soon as we have more than 1 thread, all threads are hitting the lock as
fast as they can with no gaps in between acquisition and release cycles, so the
lock is virtually never not held, so we never hit the case where the lock is
initially free and no successor concurrently appears.

When we have multiple threads and heavy contention, `abql_nosharing_lock()`
updates 2 cache lines and spins on another, and `abql_nosharing_unlock()`
updates 1 additional cache line, while `mcs_nosharing_lock()` updates (in the
common case where there are other threads enqueued) 4 cache lines, and
`mcs_nosharing_unlock()` updates 1 additional cache line in this case.  As you
can see from the graphs, rather than seeing MCS conclusively win for empty
critical sections and then seeing ABQL conclusively win for larger critical
sections (as in the paper), I see ABQL conclusively winning for empty critical
sections, inconclusive results for 1 and 10 cache line update critical
sections, and ABQL conclusively winning beyond about 14 threads for 100 cache
line update critical sections.  That said, there is enough variance in the data
(see how the graphs differ) that we can't consider any of the trends that aren't
shared between the two sets of graphs to be significant.

If you can dredge a reasonable explanation for the inconclusive range for 1 and
10 update critical sections, please let me know!  To (hopefully) help with
that, let's look at what cache lines are updated in each approach.  
* ABQL:
    * `abql_nosharing_lock()` updates 2 cache lines, and spins reading one of
      them before updating it:
        1. `queue_last`.  All threads concurrently acquiring the lock are
           contending to `lockxaddq()` this line, so it is likely to require an
invalidation round trip to the last updater to get this line in exclusive mode
and proceed. (1 round trip)
        2. `flags[my_index]`.  This line is read in a spin loop, then
           immediately written to.  Because the write occurs immediately after
receiving an updated value, this incurs (at least) a read request and response
round trip to the predecessor followed by an invalidation (read for ownership
in this case) round trip to the predecessor. (2 round trips)
    * `abql_nosharing_unlock()` updates 1 cache line: `flags[successor_index]`.
      Because another thread is either actively spinning on this cache line
waiting for its turn or previously wrote `MUST_WAIT` to it, an invalidation
round trip will be necessary to update it, followed by a read request and
response round trip to convey the updated value to the successor before the
successor may proceed. (2 round trips) 
* MCS:
    * `mcs_nosharing_lock()` updates 4 cache lines and spins reading 1 more
      when there is a predecessor, otherwise it just updates 2 cache lines.
        1. `local_lock->next`.  This line was probably updated by a
           predecessor, so it is likely to require an invalidation round trip
to the predecessor to get this line in exclusive mode and update it.  We may
proceed immediately after buffering this store. (1 round trip)
        2. `global_lock->next`. All threads concurrently acquiring the lock are
           contending to `xchgq()` this line, so it is likely to require an
invalidation round trip to the last updater to get this line in exclusive mode.
(1 round trip)
        3. If there is a successor, it updates `local_lock->locked`. Since the
           last change to this line was a predecessor writing to it, an
invalidation round trip to that predecessor will be required to get this in
exclusive mode. We may proceed immediately after buffering this store. (1 round
trip)
        4. If there is a successor, it updates `pred_lock->next`.  Since the
           last change to this line was the predecessoring writing `0` to it in
`mcs_sharing_lock()`, we need a round trip to the predecessor here.
        5. If there is a successor, it spins reading `local_lock->locked`,
           which it previously updated, so here we need one round trip for the
predecessor to invalidate this line, and then a read response from the
predecessor to provide the new value.
    * `mcs_nosharing_unlock()`:
        1. Read `local_lock->next`, which may be retained in exclusive or
           shared mode from the preceding `mcs_nosharing_lock()` call.  If not,
a read request and response round trip is needed here.
        2. If there is no successor, updates `global_lock->next`.  Acquisition
           and release contend to update this, so an invalidation round trip is
likely, and will be followed by a read request and response round trip to
another aquirer or releaser.  If `lockcmpxchgq()` succeeds, we're done.
        3. If there was no successor initially but a concurrent acquisition
           added one, spins reading `local_lock->next`.  This value has or will
be concurrently changed by our new successor, so a read request response round
trip to the successor is needed here.  
        4. If there was a successor initally or one concurrently added, updates
           `succ_lock->locked`.  The successor is probably spinning on this
line, so an invalidation round trip followed by a read request and response
round trip is required. STOPPED HERE.  Need to convert everything to counting round trips?

    To summarize, there are three paths: 
        1. A successor initially exists: here we read a cache line (which may
           be hot) and update another. 1 update total.
        2. No successor initially exists or is concurrently added: we read that
           same cache line, and update another. 1 update total.
        3. No successor initially exists, but one is concurrently added: we
           read that same cache line, attempt to update another, fail, spin on
a line that has changed or will change, then update another line.  Something
like 2 updates total.

14. **Why do you think Mellor-Crummey and Scott included 3 graphs for tests with
    "empty" critical sections and only 1 with a nonempty ("small") critical
    section?**
At least on the Butterfly and Symmetry architectures (with an empty critical
section), MCS beats ABQL.  However, at least on Symmetry when we introduce work
in the critical section, ABQL beats MCS.  Mellor-Crummey and Scott are
highlighting the situations where their approach wins and minimizing those
where it loses.
15. **BONUS: How do the `lockcmpxchgq`-based and alternative `unlock`
    implementations' performance compare?  Why?**
TODO, need to take conclusive measurements.

16. **BONUS: Explain how the alternative `unlock` implementation breaks
    fairness.**
When we initially observe no successor, but then `xchgq()` on
`global_lock->next` gives us back something besides our lock, 1) it means that
a concurrent acquisition has added a new tail, and 2) we have just unlinked
that new tail, creating an opportunity for other threads to sneak in and form a
new "usurper" queue.  If usurpers have appeared, we patch this up later by
adding our new tail to the end of the usurper queue, and this is what breaks
fairness, since the usurpers' acquisitions happened after our unlinked tail's
acquisitions (we know this because they came later in the coherence order for
`global_lock->next`), but the usurpers' critical sections will run before our
unlinked tail's critical sections, since they precede them in the queue after
we patch it up.
