# A brief primer on debugging

A variety of strange things can happen as a result of bugs in these implementations.
Often, I find that I've made a mistake somewhere, and the test harness hangs in the 
middle of a particular run, cores spinning at 100%, but no progress being made.
I've found it helpful to build with debugging symbols, and run the test harness with `gdb`
so I can inspect the program state when it gets stuck.

## Walkthrough

To get this set up, uncomment the line
```make
#DEBUG           = TRUE
```
in `Makefile`, then rebuild with `make`. 

NOTE: Your code will sometimes have _different performance characteristics_
when built this way.  Once you've resolved or learned what you needed from
debugging, remember to comment that line back out and run `make` again before
benchmarking.

Next, run the test harness attached to gdb, e.g.
```bash
gdb --args ./test 4 28 8 1
```

When the test harness hits the loop, stop the program and return control to
`gdb` with a `SIGINT` (`CTRL-c`).  Check out your threads with
```gdb
info threads
```
select one that looks like it's running your code, and switch to it, e.g. for thread 2
```gdb
t 2
```
and see where we are in the code.
```gdb
list
```
NOTE: This listing has macros expanded, and expansions often look very
different from the macro name.  Sometimes this has the benefit of helping you
figure out exactly where the program counter is in an expansion of a multi-line
macro, other times it just makes it hard to read the code.  Perhaps a future
assignment will include a way to turn this off :)

Once we know where the program counter is (which must be code along the path of
the infinite loop, if this thread is in fact looping forever), it might be 
helpful to print the values of some variables in scope, e.g. for `ticket_lock()`
```gdb
p *lock
```
```gdb
p my_number
```
or for `abql_nosharing_lock()`
```gdb
p *my_place
```
```gdb
p flags[0]
```

Usually I do this, and then stare at the implementation for a while in my editor, trying
to figure out how the state ended up this way.

## Other things to do

### Inspect frames of calling functions

If you want to inspect values in scope in a function call further up the stack,
you can check out the stack,
```gdb
bt
```
pick a different frame to inspect, and load it, e.g. for frame 2
```
f 2
```

And print some variables in scope in the calling function.

### Breakpoints/Watchpoints and Invariants

You can set breakpoints and watchpoints, and do other GDB things more or less
as usual.  If you are feeling paranoid about the impact of making a system call
when you hit a watchpoint/breakpoint, or just can check what you want to more
easily in C, you can add a runtime invariant that loops forever if violated to
the code.

Instead of a breakpoint:
```c
while(1) {}
```

Instead of a watchpoint:
```c
if(!invariant) { while(1) {} }
```

Run it through `gdb` and see if any threads get stuck in this particular loop.
If one did, you can print some stuff in scope at that line, and figure out how
the invariant was violated.  The disadvantage of this approach, of course, is
that it's harder to resume execution afterwards (if you care about that).
