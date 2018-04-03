# Assignment 4: The Linux-Kernel Memory Model

**Due Date: 4:19pm on Thursday, March 8th**

## Overview

In this assignment, we're exploring a subset of the Linux-Kernel Memory Model, introduced
by [Alglave, Maranget, McKenney, Parri, and Stern 2017](https://lwn.net/Articles/718628/) 
and continued
[here](https://lwn.net/Articles/720550/), with further examples
[here](http://web.cecs.pdx.edu/~walpole/class/cs510/winter2018/linuxmm/Examples.html),
and summarized by 
[Stern 2017](https://github.com/aparri/memory-model/blob/master/Documentation/explanation.txt).
We're looking at this model to get a better understanding of how memory
accesses and barriers work in a cross-platform setting, and get some practice
visualizing the happens-before relation (and others) to explain which behaviors
are allowed and which are not.  Remember that one consequence of developing a
cross-platform memory model is that if some behavior (such as reordering
particular memory accesses relative to one-another) is allowed by any
architecture that runs Linux, it must be allowed by the memory model, even if
other architectures that run Linux don't allow that behavior.  As a result,
we'll have to reason about the code we see in this assignment in keeping with a
different set of rules that we've been using to reason about the code we've
written in previous assignments to run on `x86_64`, which allows comparatively
fewer reorderings than most architectures that run Linux.
Take a look at 
[this table](https://en.wikipedia.org/wiki/Memory_ordering#In_symmetric_multiprocessing_(SMP)_microprocessor_systems)
to get an idea of what reorderings happen on modern architectures, and just how
conservative in this respect `x86_64` (listed as `AMD64`) is compared to an
architecture like `POWER` or `ARMv7`, both of which also run Linux.

The series of steps in this assignment build on one-another only conceptually,
so you can complete the implementations in any order to get things working,
although it may be confusing to go out of order.  There are numbered questions
throughout the assignment; we recommend thinking through these questions
carefully and writing down short concrete answers before moving on to the next
section.  If you're confused by a question or don't know where to start, you're
free to discuss or puzzle through them with other students, and Ted and Jon are
also available to help.  The point of these assignments is to develop your
understanding of the material and give you some hands-on intuition for
concurrent programming, and the questions are there to help you understand
what's going on.  If they don't, we'd really like to know, so we can write some
better ones :)

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
pushd Assignment_4
pushd data
# remove all data and graphs BESIDES the final ones you'd like me to look at
rm <out-of-date data and graphs>
popd
make clean
popd
# where NAME is your name separated by underscores
tar czf Assignment_4-NAME.tar.gz Assignment_4/
```
 
Email the archive to [theod@pdx.edu](mailto:theod@pdx.edu) with subject
`Assignment_4-NAME` and any questions and comments you'd like to share.
Include your answers to the numbered questions in the assignment in the text of
the email.

### Litmus Tests and Relations on Events

We'll be exploring a series of litmus tests in this assignment.  Litmus tests
are small concurrent programs that perform memory accesses and other operations
that affect those accesses, usually with multiple threads accessing shared
data.  At the end of each test is a postcondition on the state of memory after
all threads have completed.  Often, these postconditions check the final values of
registers whose values are loaded from shared variables, to determine what values those
variables can take on at a particular time from a particular thread's perspective.  

The herd memory model simulator 
(introduced in [Alglave, Maranget, and Tautschnig 2014](https://arxiv.org/pdf/1308.6810)) 
builds a relational model of the program in the litmus test, where each access
or operation is an event, and various relations exist between the events.  For example, if
one thread writes `1` to `x` and another reads `x` and observes `1` (and
no other thread also writes a `1` to `x`), then we say that there is a
"reads-from" (`rf`) edge from the write to the read.  Refer to
[Stern 2017](https://github.com/aparri/memory-model/blob/master/Documentation/explanation.txt)
for a good overview of the basics.

herd enumerates all possible ways of wiring up these relations for a litmus test,
checks whether the postcondition holds for relations that correspond to executions that
could actually happen under the memory model,
and throws out possibilities that couldn't happen under the memory model.
For the Linux-Kernel Memory Model (see `linux-kernel.cat` for the actual definitions and acylicity checks):
*  herd rejects wirings that violate sequential consistency per variable.
   These are wirings where there is a cycle in the union of relations that
   represent reads and writes to a particular variable, i.e.  the union of
   relations `po-loc` (accesses on a variable within a single thread), `rf` (edges
   from a write to a read that observes the value written), `co` (total order on
   writes to the variable), and `fr` (edges from a read to a write that read
   missed).  I've labeled this union `cb`.  In the model, this check is called
   `coherence`.
*  herd rejects wirings where atomic read-modify-write instructions observably
   don't execute atomically, (e.g. for a compare and swap, the variable takes
   on a new value in its coherence order in between the value we compared against
   and the value we're swapping in).  In the model, this check is called `atomic`.
   We won't be using any atomic instructions in this assignment.
*  herd rejects wirings where there is a cycle in the temporal happens before
   relation.  This relation is pretty large, including events in preserved
   program order `ppo`, reads from external writes (writes made by another thread)
   `rfe`, and a subset of the propagation order on writes within a thread.  In the
   model, this check is called `happens-before`.
*  herd rejects wirings where there is a cycle in the propagation of writes
   `prop` preceding a strong fence and followed by any number of happens before
   `hb` edges.  In the model, this check is called `propagation`.
*  Finally, herd rejects wirings where the `rcu-path` relation creates an edge
   from an event to itself (i.e. it checks that the relation is irreflexive).
   We won't be using any of the events that build this relation in this
   assignment.

For the remaining wirings for a litmus test, herd checks whether the
postcondition holds.  herd can check postconditions formulated using the
following quantifiers:
*  `exists` (there is some valid wiring whose final state matches the
   condition), 
*  `~exists` (there is no valid wiring whose final state matches the
   condition), and 
*  `forall` (every valid wirings's final state matches the condition).  We will
   generally use the `exists` quantifier for this assignment.

For more information about using herd, check out the documentation 
[here](http://diy.inria.fr/doc/herd.html).

The point of a test is to create the smallest possible example to illustrate a
particular idea about what is (or is not) allowed by the memory model, to give
programmers a better understanding of how to write concurrent code using the
model.  The tests below will develop our intuition about when writes appear to
be ordered and when they don't.

### Using herd 

Babbage has all of the ocaml jazz we need to build and use herd, we just need
to get the tool itself up and running.

I have built a version of herd that works with the memory model in my home directory.
You can set up your shell to use it as follows:

```bash
export PATH="/u/theod/bin:$PATH"
```

If you have issues getting this to work, or prefer to build your own copy in
your home directory, you can do it like so:

```bash
cd ~/git
git clone https://github.com/herd/herdtools7.git
cd herdtools7
git checkout 44d69c2b1b5ca0f97bd138899d31532ee5e4e084
make all
make install
export PATH="$HOME/bin:$PATH"
```

For more information on building herd, check out the project's
[INSTALL.md](https://github.com/herd/herdtools7/blob/master/INSTALL.md).

I've included some scripts in the `Assignment_4` directory that supply various parameters
to herd to make it do different things: `./check`, `./graph`, and `./failgraph`.

## Exploring Litmus Tests

### Coherence Order

We'll start by exploring the implications of having a total coherence order on
one variable `x`, using this as an opportunity to get to know the tools.

#### One variable, one writer, one reader: `CO+wx-wx+rx-rx`

Here are the contents of `litmus-tests/CO+wx-wx+rx-rx1.litmus`:

```
C CO+wx-wx+rx-rx1

{}

P0(int *x)
{
  WRITE_ONCE(*x, 1);
  WRITE_ONCE(*x, 2);
}

P1(int *x, int *y)
{
  int r0;
  int r1;

  r0 = READ_ONCE(*x);
  r1 = READ_ONCE(*x);
}

exists (1:r0=1 /\ 1:r1=2)
```

It's a simple program with two threads (`P0` and `P1`).  `P0` writes two
successive values to `x`, and `P2` reads `x` twice.  From the write-write
coherence rule (see `CACHE COHERENCE AND THE COHERENCE ORDER RELATION` in
[Stern 2017](https://github.com/aparri/memory-model/blob/master/Documentation/explanation.txt)),
we know that because `P0`'s write of 1 to `x` precedes its write of 2 to `x` in
program order, and both writes are to the same variable, the first write must
precede the second in the coherence order for `x`.  This means that when we use
herd to `./check` whether the postcondition holds for some wiring (this one looks
for a wiring where `P1`'s variable `r0` has the value 1 and `P1`'s variable
`r1` has the value 2), we should get something like the following output:

```bash
$ ./check litmus-tests/CO+wx-wx+rx-rx1.litmus
Test CO+wx-wx+rx-rx1 Allowed
States 6
1:r0=0; 1:r1=0;
1:r0=0; 1:r1=1;
1:r0=0; 1:r1=2;
1:r0=1; 1:r1=1;
1:r0=1; 1:r1=2;
1:r0=2; 1:r1=2;
Ok
Witnesses
Positive: 1 Negative: 5
Condition exists (1:r0=1 /\ 1:r1=2)
Observation CO+wx-wx+rx-rx1 Sometimes 1 5
Time CO+wx-wx+rx-rx1 0.01
Hash=4245322cd6d4c0585c466fa11d18657c
```

The first part of the output enumerates all of the distinct final states 
herd has found that correspond to valid wirings.  Next, herd prints `Ok` to
indicate that the postcondition holds (otherwise it prints `No`), counts how
many of the final states satisfy it, prints the condition, and prints what looks
like the contents of an algebraic data type inhabitant whose second field
indicates whether for valid wirings the condition holds `Always`, `Never`, or
(as in this case) `Sometimes`.

Reading through the states above, we can see that there are no states where
`P1` has observed `x` to be 2 before observing it to be 1.  Knowing that the
model enforces write-write coherence, this isn't surprising.  Let's see what
wiring corresponds to the condition above (where `P1`'s first read sees `P0`'s
first write, and `P1`'s second read sees `P0`'s second write):

```bash
$ ./graph litmus-tests/CO+wx-wx+rx-rx1.litmus
Test CO+wx-wx+rx-rx1 Allowed
States 6
1:r0=0; 1:r1=0;
1:r0=0; 1:r1=1;
1:r0=0; 1:r1=2;
1:r0=1; 1:r1=1;
1:r0=1; 1:r1=2;
1:r0=2; 1:r1=2;
Ok
Witnesses
Positive: 1 Negative: 5
Condition exists (1:r0=1 /\ 1:r1=2)
Observation CO+wx-wx+rx-rx1 Sometimes 1 5
Time CO+wx-wx+rx-rx1 0.01
Hash=4245322cd6d4c0585c466fa11d18657c

Warning: node 'eiid0', graph 'G' size too small for label
Warning: node 'eiid1', graph 'G' size too small for label
Warning: node 'eiid2', graph 'G' size too small for label
Warning: node 'eiid3', graph 'G' size too small for label
```

This generates a graph of the (first?  only, in this case) wiring that
satisfies the condition in `pdf/CO+wx-wx+rx-rx1.pdf`.  Open the pdf, and follow
along with this paragraph.  The column output functionality for graphs seems to
be broken in this version of herd, but we can see `P0`'s two write events,
labeled `a: W[once]x=1` (corresponding to `WRITE_ONCE(x,1)`), and `b:
W[once]x=2` (corresponding to `WRITE_ONCE(x,2)`), and see the blue coherence
order edge (`co`) from `a` to `b`.  We can also see `P1`'s two read events,
labeled `c: R[once]x=1` (corresponding to `r0 = READ_ONCE(x)`, where the value
of `x` observed is 1), and `d: R[once]x=2` (corresponding to `r1 =
READ_ONCE(x)`, where the value of `x` observed is 2), and see the black program
order edge (`po`) from `c` to `d`.  These observations are consistent with the
red reads from (`rf`) edges from `a` to `c` and `b` to `d`, and the orange from
reads (`fr`) edge from `c` to `b`, indicating that the read `c` missed the
write `b`.  Observe that this graph has no cycles, so it clearly passes the
memory model's `coherence`, `happens-before`, and `propagation` cycle checks.

To be *really sure* that there isn't a valid wiring where `P1` observes `x` at
`2` and then at `1`, though, let's make that the condition, and see what
happens.  There is a litmus test with the same code, but the following
condition, in `litmus-tests/CO+wx-wx+rx-rx2.litmus`:

```
exists (1:r0=2 /\ 1:r1=1)
```

If we check it, we see that it fails:

```bash
$ ./check litmus-tests/CO+wx-wx+rx-rx2.litmus
Test CO+wx-wx+rx-rx2 Allowed
States 6
1:r0=0; 1:r1=0;
1:r0=0; 1:r1=1;
1:r0=0; 1:r1=2;
1:r0=1; 1:r1=1;
1:r0=1; 1:r1=2;
1:r0=2; 1:r1=2;
No
Witnesses
Positive: 0 Negative: 6
Condition exists (1:r0=2 /\ 1:r1=1)
Observation CO+wx-wx+rx-rx2 Never 0 6
Time CO+wx-wx+rx-rx2 0.01
Hash=4245322cd6d4c0585c466fa11d18657c
```

Note `No` and `Never` in the output to confirm this.  Use the failgraph tool to
take a look at a graph that satisfies the postcondition but is rejected by the
model (hence the `No` result above), and note our first problematic cycle:

```bash
./failgraph litmus-tests/CO+wx-wx+rx-rx2.litmus
```

This cycle is in `cb`, indicating that coherence has been violated.  We can see
the cycle, an `rf` edge from `b` (`P0`'s second write to `x`) to `c` (`P1`'s
first read of `x`, which we have required to observe the second write in the
postcondition), then a `po-loc` edge from `c` to `d` (`P1`'s second read of
`x`, which we have required to observe the first write in the postcondition,
finishing our knot in the coherence order), and finally a `fr` edge from `d`
back to `b`, since this read missed the write of 2 to `x`.

#### One variable, two writers, one reader: `CO+wx+wx+rx-rx`

Now let's check out `litmus-tests/CO+wx+wx+rx-rx1.litmus`:

```
C CO+wx+wx+rx-rx1

{}

P0(int *x)
{
  WRITE_ONCE(*x, 1);
}

P1(int *x)
{
  WRITE_ONCE(*x, 2);
}

P2(int *x)
{
  int r0;
  int r1;

  r0 = READ_ONCE(*x);
  r1 = READ_ONCE(*x);
}

exists (2:r0=2 /\ 2:r1=1)
```

Here, we've separated the writes to `x` from the previous litmus test into two
threads.  Now, there isn't any guarantee which will make it first into `x`'s
coherence order.  Verify this by checking the test above

```bash
./check litmus-tests/CO+wx+wx+rx-rx1.litmus
```

and then checking another litmus test with the same program, but a condition

```
exists (2:r0=1 /\ 2:r1=2)
```

that looks for `P0`'s write to happen first (instead of `P1`'s, as above)

```bash
./check litmus-tests/CO+wx+wx+rx-rx2.litmus
```

Use `./graph` to generate graphs for each, and observe that the main difference
is that the two write events `a` and `b` switch places.  Now that program order
isn't forcing them into the coherence order in any particular... order, they
are free to be observed by `P2` in either (sigh) order.

#### One variable, two writers, two readers: `CO+wx+wx+rx-rx+rx-rx1`

It might be easy to look at the result above
(that a thread can observe either write to `x` first), and assume that two threads
might observe the writes in different orders.  But remember, 
[Stern 2017](https://github.com/aparri/memory-model/blob/master/Documentation/explanation.txt)
also mentions that we have read-read coherence, where two reads in program order to the same
variable have to line up with the coherence order for that variable.  So, if there's only one
coherence order for a variable, and loads have to agree with it in program order, we should see
two threads' loads of a variable agree, right?

Check out `litmus-tests/CO+wx+wx+rx-rx+rx-rx1.litmus` for an example where this is the case:

```
C CO+wx+wx+rx-rx+rx-rx1

{}

P0(int *x)
{
  WRITE_ONCE(*x, 1);
}

P1(int *x)
{
  WRITE_ONCE(*x, 2);
}

P2(int *x)
{
  int r0;
  int r1;

  r0 = READ_ONCE(*x);
  r1 = READ_ONCE(*x);
}

P3(int *x)
{
  int r2;
  int r3;

  r2 = READ_ONCE(*x);
  r3 = READ_ONCE(*x);
}

exists (2:r0=1 /\ 2:r1=2 /\ 3:r2=1 /\ 3:r3=2)
```

We might as well do the checking and graphing in one fell swoop:

```bash
./graph litmus-tests/CO+wx+wx+rx-rx+rx-rx1.litmus
```

Note that we get an `Ok`, so at the very least, there is some valid wiring
where threads agree on the sequence of values `x` takes on.  Check out the
graph this generates, with red `rf` edges from the first write to both first
reads, and from the second write to both second reads.  This example feels like
the platonic ideal of agreement with the coherence order: everything lines up
nicely and all threads see the same thing.  If you'd like to acquaint yourself
with other examples where the readers still agree with the coherence order,
pick a state enumerated in herd's output (or imagine one you would like to see)
and change the postcondition in the litmus test, then re-run `./graph` and see
what you have wrought.

We may have some pretty strong intution at this point that reads agree with the
coherence order, but let's see if we can find a case where they don't.  Really,
what we want to check is that some ordering of `r0`, `r1`, `r2`, and `r3` where
`r0` precedes `r1` and `r2` precedes `r3` agrees with a coherence order for
`x`, in which either `2` precedes `1` or `1` precedes `2`. However, we'll have
to be satisfied with showing that one example where this property fails does
not correspond to a valid wiring, and then examining the states herd enumerates
to check that in each case, this property holds.

The example searches for a wiring that satisfies

```
exists (2:r0=1 /\ 2:r1=2 /\ 3:r2=2 /\ 3:r3=1)
```

This is clearly not in keeping with `x` having a coherence order that both observers
agree on, since `P2` has observed 1 preceding 2, while `P3` has observed 2 preceding 1.
There would have to be a cycle in the coherence order for `x` if both observations were
possible, and as 
[Stern 2017](https://github.com/aparri/memory-model/blob/master/Documentation/explanation.txt)
points out in `ORDERING AND CYCLES`, an ordering cannot contain a cycle.

Let's check this understanding by running a litmus test with the same code as above
that searches for this wiring:

```bash
./check litmus-tests/CO+wx+wx+rx-rx+rx-rx2.litmus
```

Verify that you see a `No` for this particular case, and then scan through the
enumerated states, searching for one where `P2` and `P3` disagree about the
order of values that `x` takes on.  You should not be able to find one.  For
example, in the state
```
2:r0=2; 2:r1=1; 3:r2=0; 3:r3=2;
```
both perspectives are consistent with `x` having taken on `0`, `2`, and then
`1`, while in the state 
```
2:r0=0; 2:r1=2; 3:r2=1; 3:r3=1;
```
both perspectives are consistent with `x` having taken on `0`, `1`, `2`, and
(alternatively) with `x` having taken on `0`, `2`, `1`.

Find the `cb` cycle in the resulting graph:

```bash
./failgraph litmus-tests/CO+wx+wx+rx-rx+rx-rx2.litmus
```

Once again, the cycle includes the write that updates `x` to 2, `P2`'s read that
observes it at 2, and `P2`'s po-later read that observes it at 1, violating the
coherence order for `x` the `P1` observed in `c` and `d`.

##### Keeping things readable

If the graphs get overwhelming to look at, turn off edges you aren't interested
in in the `doshow` line in `linux-kernel.cfg`.  Similarly, if there are edges
in the model that you'd like to see, turn them on.  At this point, we're done
looking at coherence violations, so you can safely turn off the `cb` edges.

#### Two variables, one writer, one reader: `MP+wx-wy+ry-rx1`

First, let's show that when we have one thread writing to two
different variables (`x` and `y`), another can observe these writes
in either order:

```
C MP+wx-wy+ry-rx1

{}

P0(int *x, int *y)
{
  WRITE_ONCE(*x, 1);
  WRITE_ONCE(*y, 1);
}

P1(int *x, int *y)
{
  int r0;
  int r1;

  r0 = READ_ONCE(*y);
  r1 = READ_ONCE(*x);
}

exists (1:r0=0 /\ 1:r1=1)
```

```bash
./graph litmus-tests/MP+wx-wy+ry-rx1.litmus
```

Here, we see the write to `x` happen first from `P1`'s perspective.

Well, can we see the write to `y` happen first?  Here's the same code,
with the postcondition

```
exists (1:r0=1 /\ 1:r1=0)
```

```bash
./graph litmus-tests/MP+wx-wy+ry-rx2.litmus
```

This also corresponds to a valid wiring.  Take a look at the graph
(`pdf/MP+wx-wy+ry-rx2.pdf`) to note a benign cycle.  Note that the
cycle includes `po` (program) edges between reads, while the `hb` relation in
`linux-kernel.cat` (which the model requires to be acyclic) only includes `ppo`
(preserved program order) edges.  Review the `THE PRESERVED PROGRAM ORDER RELATION` section
in
[Stern 2017](https://github.com/aparri/memory-model/blob/master/Documentation/explanation.txt)
to get some clarity on the distinction between `po` and `ppo`.

##### Questions
1.  Are the `po` edges we see in the graph in `ppo`?  Why or why not?
2.  From an operational perspective, your answer to 1 amounts to a statement
    about whether adjacent loads of different variables can be reordered in
this memory model.  What is that statement?
3.  Is your answer to 2 true for all architectures supported by the memory
    model?  If not, give an example of a supported architecture where it is
true, and a supported architecture where it is false.

So, we've established that an observer can see these writes in either order.
What if we don't want that?

##### Making the write to `x` happen before the write to `y`

What if we want a guarantee that the observer will see the write to `x`
happening before the write to `y`?  This could be useful if, for instance, the
first write populated a field in a struct, and the second write published a
pointer to that struct.  If another thread could observe the new pointer but
miss the field update, that thread could see the struct in a
not-yet-initialized state, possibly resulting in an error.  To avoid that, we
need to make sure the first write is always visible to anyone observing the
second write.  This is an example of the message passing paradigm, where the
first write establishes a message payload, and the second write shares that
payload with observers.  Because the writes are related at this semantic level,
it makes sense for us to guarantee that they are only observed in an order that
fulfills the semantics we need.  We can make this happen, we just need to
include some explicit guidance, so the machine knows what we want.

Recall a couple of definitions from 
[Stern 2017](https://github.com/aparri/memory-model/blob/master/Documentation/explanation.txt):

```
smp_rmb() forces the CPU to execute all po-earlier loads
before any po-later loads;

smp_wmb() forces the CPU to execute all po-earlier stores
before any po-later stores;

...

When a fence instruction is executed on CPU C: 
For each other CPU C', smb_wmb() forces all po-earlier stores
on C to propagate to C' before any po-later stores do.
```

Use these barriers, and the rest of what you know from 
[Stern 2017](https://github.com/aparri/memory-model/blob/master/Documentation/explanation.txt)
to modify the program to guarantee that the following postcondition always holds:

```
forall ((1:r0=0 /\ 1:r1=0) \/ (1:r0=0 /\ 1:r1=1) \/ (1:r0=1 /\ 1:r1=1))
```

All this says is that if the write to `y` is not observed, then the write to
`x` may or may not be observed, but if the write to `y` is observed, then the
write to `x` must be observed too.

To be clear, we're looking for output with `Ok` that shows that the condition
`Always` holds.

##### Questions
4.  What barrier(s) did you have to add (and where) to make this work?
5.  For each barrier you added, explain why it was necessary to do so, and
what would happen if you hadn't.
6.  If two writes to different variables propagate to a CPU in some order, does
that guarantee that the CPU will observe them in that order?

##### Why are other wirings ruled out?

Change the postcondition back to the second one we checked (where the write to
`y` is observed first) and 

```
exists (1:r0=1 /\ 1:r1=0)
```

but keep the barriers as they are.  Save this version of the file as
`litmus-tests/MP+wx-wy+ry-rx3.litmus`.  Now, we can use the `failgraph` script
to show us another cycle, in a graph that satisfies this new postcondition but
isn't allowed by the memory model.  

```bash
./failgraph litmus-tests/MP+wx-wy+ry-rx3.litmus
```

Take a look at the graph, and answer the following:

##### Questions
7.  We see a series of edges in this graph where you have two accesses
    separated in program order by a barrier.  Are these accesses in `ppo`?  Why
    or why not?
8.  It's clear that there's a happens before cycle between `P1`'s reads, but
    it may not be obvious why it's there.  Let's track it down:
    1. Consider the `fr` edge from `P1`'s read of `x` at 0 to `P0`'s write to
       `x` of 1.  Has the write propagated to `P1` when the read observes `x`?
    2. Tell herd to show us edges in the `prop-irrefl` and `prop-local`
       relations by editing `linux-kernel.cfg` and adding them to the list on
       the line starting with `doshow`.  A `prop-local` edge  exists when a chain of 
       one or more `prop-irrefl` edges (which can span CPUs) ends up linking two
       events on the same CPU.  Every `prop-local` edge is also a `hb` edge,
       so we can use the acyclicity of happens before to maintain consistency
       with a variable's coherence order as the variable takes on new values 
       concurrently with local accesses to it.
       ([Stern 2017](https://github.com/aparri/memory-model/blob/master/Documentation/explanation.txt)
       elaborates on this, calling our `prop-local` `prop`, in `THE
       HAPPENS-BEFORE RELATION` section).  
       Looking at the new graph, why does `d` happen before `c`?  
    3. Why does `c` happen before `d`?

#### Two variables, one writer, two readers: `MP+wx-wy+rx-ry+ry-rx1`

Let's add an observer and go back to the drawing board.  Since one observer can
see the writes happen in either order, it stands to reason that this will also
be the case for two observers.  A separate question is whether the observers
will always agree on the order of writes.  In `CO+wx+wx+rx-rx+rx-rx1` above, we
saw that when the writes are to the same variable, observers always agree.
What about when they're to different variables?  Let's set up a program to test
this, and see if we can get observers to disagree.

Check out `litmus-tests/MP+wy-wx+rx-ry+ry-rx1.litmus`:

```
C MP+wy-wx+rx-ry+ry-rx1

{}

P0(int *x, int *y)
{
  WRITE_ONCE(*y, 1);
  WRITE_ONCE(*x, 1);
}

P1(int *x, int *y)
{
  int r0;
  int r1;

  r0 = READ_ONCE(*x);
  smp_rmb();
  r1 = READ_ONCE(*y);
}

P2(int *x, int *y)
{
  int r2;
  int r3;

  r2 = READ_ONCE(*y);
  smp_rmb();
  r3 = READ_ONCE(*x);
}

exists (1:r0=1 /\ 1:r1=0 /\ 2:r2=1 /\ 2:r3=0)
```

Here we're checking whether `P1` can observe the write to `x` first, while `P2`
observes the write to `y` first.  Note the `smb_rmb()`s between the reads,
which guarantee that they aren't reordered locally.  This means that if `P1`
and `P2` disagree on the order of writes to `x` and `y`, it's because the
writes themselves aren't in an order that observers have to agree on.  Let's
run the check, and if it passes generate a graph:

```bash
./graph litmus-tests/MP+wy-wx+rx-ry+ry-rx1.litmus
```

This case does occur, and a graph illustrating its corresponding wiring is
generated.  In other words, the memory model allows `P1` and `P2` to disagree
about the order of writes to `x` and `y`!

There are a couple of related things to note in the graph.  One is that we have
another benign cycle, that is once again allowed because neither the from reads
(`fr`) edges nor program order (`po`) edges are happens before (`hb`) edges.
Another is that `rf` and `fr` edges connect `P0`'s writes to `P1`'s reads in
basically the opposite way that they connect `P0`'s writes to `P2`'s reads, and
that this is what creates the benign cycle we just noted.

Again, let's "fix" this.  Use barriers (more or less as before), to guarantee
that both threads see the write to `x` happen first.  Write this new test to
`litmus-tests/MP+wy-wx+ry-rx+ry-rx2.litmus`.  Now the postcondition should
fail:

```bash
./check litmus-tests/MP+wx-wy+rx-ry+ry-rx2.litmus
```

Now, generate a graph of a wiring that satisfies the postcondition but is ruled
out by the memory model:

```bash
./failgraph litmus-tests/MP+wx-wy+rx-ry+ry-rx2.litmus
```

##### Questions
9.  What barrier(s) did you have to add, and where?
10. Where is the cycle?  Why does the memory model rule it out?

##### Which check failed?

I haven't found a way to make herd print which cycle check failed for a
particular ruled out wiring.  However, it does provide a way to skip specified
checks in the memory model, and when we have a postcondition that only
corresponds to wirings that fail the checks, we can figure out which check(s)
some wiring that satisfies it is failing by disabling subsets of checks
one-by-one until it stops failing.  For example, to skip the memory model's
`happens-before` check on a litmus test, you can use the following incantation:

```bash
herd7 -conf linux-kernel.cfg -skipcheck happens-before litmus-tests/MP+wy-wx+ry-rx+ry-rx2.litmus 
```

You can disable multiple checks as follows:

```bash
herd7 -conf linux-kernel.cfg -skipchecks happens-before,propagation,coherence,rcu litmus-tests/MP+wy-wx+ry-rx+ry-rx2.litmus
```

If you'd like to guarantee that exactly the specified check(s) fail (and all
others pass), set the `strictskip` flag.  So, to check whether *only* happens-before fails:

```bash
herd7 -conf linux-kernel.cfg -skipcheck happens-before -strictskip true litmus-tests/MP+wy-wx+ry-rx+ry-rx2.litmus 
```

#### Two variables, two writers, two readers: `IRIWish+rx-ry+wx+wy+ry-rx1`

Now, let's separate the writes to `x` and `y` into separate threads, and see
what it takes to create an ordering between them.  First, we once again confirm
that the observers (`P0` and `P3` this time around) can disagree on the order
of writes:

```
C IRIWish+rx-ry+wx+wy+ry-rx1

{}

P0(int *x, int *y)
{
  int r0;
  int r1;

  r0 = READ_ONCE(*x);
  smp_rmb();
  r1 = READ_ONCE(*y);
}

P1(int *x)
{
  WRITE_ONCE(*x, 1);
}

P2(int *y)
{
  WRITE_ONCE(*y, 1);
}

P3(int *x, int *y)
{
  int r2;
  int r3;

  r2 = READ_ONCE(*y);
  smp_rmb();
  r3 = READ_ONCE(*x);
}

exists (0:r0=1 /\ 0:r1=0 /\ 3:r2=1 /\ 3:r3=0)
```

```bash
./graph litmus-tests/IRIWish+rx-ry+wx+wy+ry-rx1.litmus
```

Unsurprisingly, the postcondition holds.  Check out the graph, noting a similar
benign cycle to those we've seen before.  

Next, let's try and recreate the situation we had in the last example, where
everyone agrees that the write to `x` happens before the write to `y`.  We
can't put an `smp_wmb()` in between the writes anymore, since they're in
different threads now.  Besides, we know from the end of `AN OPERATIONAL MODEL`
in 
[Stern 2017](https://github.com/aparri/memory-model/blob/master/Documentation/explanation.txt)
that `smp_wmb()` only orders writes local to the CPU that executes it; it does
not guarantee that a write from another CPU observed before `smp_wmb()` will
propagate to other CPUs before a local write after `smp_wmb()` propagates to
them.

One thing we can do that seems like it might work is to make `P2` read `x`, and
write whatever value it observes `x` having to `y`.  In other words, we're
creating a `data` dependency from `P2`'s read of `x` to its write to `y`.  Copy
the value read from `x` into a register (which you'll need to declare), then
store that to `y` (instead of 1).  If `P2` sees the write to `x`, then *surely*
that write is visible to other threads, too.  You'll need to add another
parameter `int *x` to `P2`'s signature.  Write this new test to
`litmus-tests/IRIWish+rx-ry+wx+wy+ry-rx2.litmus`.  Let's check that this creates
an ordering between the updates to `x` and `y`:

```bash
./check litmus-tests/IRIWish+rx-ry+wx+wy+ry-rx2.litmus
```

Wait, what?  We know that `P2` observed `P1`'s write to `x` because `P3`
observed `y` at 1, but then (in `ppo`) `P3` missed `P1`'s write to `x`,
observing `x` at 0.  How is this possible?  First, let's add the following to
`linux-kernel.cfg` to produce a more readable graph, where the `rmb` events are
removed (there are still `ppo` edges where they were):

```
unshow po
showevents mem
```

Generate the graph to see this madness in action:

```bash
./failgraph litmus-tests/IRIWish+rx-ry+wx+wy+ry-rx2.litmus
```

Well, at least the `data` dependency is there.  Trace the `hb` edges, and note
that there is no cycle.  Also note that if there were a `prop-irrefl` edge from
`d` to `e`, we'd have a `prop-local` edge from `g` to `f` (and thus an `hb`
edge and an `hb` cycle).  At a high level, this is an expression of the fact
that a `data` dependency, even though it gives us `ppo`, does not ensure that the
write we read from is propagated to other CPUs before the write that depends on
the read!  In this example, it means that it's perfectly acceptable for `P3` to
see the value of `y` that came from `P1`'s update to `x` without seeing that
same update to `x`.  

Intuitively, this does make a certain amount of sense: Just because a write has
propagated to our CPU, should we be able to depend on it having propagated to
all other CPUs?  In previous examples with two variables and one writer, we
only had to ensure that a single writer's writes were propagated in order.
Now, we have to ensure that a write from some *other* CPU is fully propagated
before our write.  This may involve some amount of waiting to hear back from
other CPUs, so rather than make it the default behavior, many architectures
provide special barriers and memory accesses for this purpose.
[Stern 2017](https://github.com/aparri/memory-model/blob/master/Documentation/explanation.txt)
talks about this at the end of `AN OPERATIONAL MODEL`, drawing the distinction
between events that are *A-cumulative* (requiring writes that have previously
propagated to this CPU to propagate to all CPUs) and those that are not.  There
are more detailed definitions of *A-* and *B-cumulativity* in 
[Alglave et al 2017 (A Strong Formal Model of Linux-Kernel Memory Ordering
)](https://www.kernel.org/pub/linux/kernel/people/paulmck/LWNLinuxMM/StrongModel.html).

In that section, Stern points out that `smp_store_release()` *is* A-cumulative.
Modify `P2` to use this for its store (the first parameter is a pointer, not an
int lvalue as in `WRITE_ONCE()`, the second parameter is the value to store).
Write this version to `litmus-tests/IRIWish+rx-ry+wx+wy+ry-rx3.litmus`.

Confirm that we can no longer observe the writes out of order:

```bash
./check litmus-tests/IRIWish+rx-ry+wx+wy+ry-rx3.litmus
```

Next, generate a graph for an invalid wiring that satisfies the condition:

```bash
./failgraph litmus-tests/IRIWish+rx-ry+wx+wy+ry-rx3.litmus
```

##### Questions
11. Where is the cycle?  What has changed in the graph to make it appear?
12. BONUS:  There is at least one other way, using the tools Stern introduces,
    to make this postcondition fail (without using `smp_store_release()`).
    Find one, and explain it.
13. Broadly, what (beyond program order) does it take: 
    1. to order two writes to the same variable by the same CPU?
    2. to order two writes to different variables by the same CPU?
    3. to order two writes to different variables by different CPUs?

Copyright Ted Cooper, February 2018
