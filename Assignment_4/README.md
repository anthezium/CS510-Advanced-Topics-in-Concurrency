# Assignment 4: The Linux-Kernel Memory Model

**Due Date: 4:19pm on Thursday, February 22nd**

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
are allowed and which are not.

The series of steps in this assignment build on one-another only conceptually,
so you can complete the implementations in any order to get things working
(just compare both of them once you're done).  There are
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
For the Linux-Kernel Memory Model (see `linux-kernel.cat`):
*  herd rejects wirings that violate sequential consistency per variable.
   These are wirings where there is a cycle in the union of relations that
   represent reads and writes to a particular variable, i.e.  the union of
   relations `po-loc` (accesses on a variable within a single thread), `rf` (edges
   from a write to a read that observes the value written), `co` (total order on
   writes to the variable), and `fr` (edges from a read to a write that read
   missed).  In the model, this check is called `coherence`.
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

Note `No` and `Never` in the output to confirm this.

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
pretty symmetric graph this generates (a fortune cookie?), with red `rf` edges
from the first write to both first reads, and from the second write to both
second reads.  This example feels like the platonic ideal of agreement with the
coherence order: everything lines up nicely and all threads see the same thing.
If you'd like to acquaint yourself with other, less pastoral examples where the
readers still agree with the coherence order, pick a state enumerated in herd's
output (or imagine one you would like to see) and change the postcondition in
the litmus test, then re-run `./graph` and see what you have wrought.

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

#### Two variables, one writer, two readers: 


Copyright Ted Cooper, February 2018
