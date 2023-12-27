# FAQ: A garbage collector for C and C++

This is the beginning of a "frequently asked questions" file for what has
become known as the "Boehm-Demers-Weiser" garbage collector.  Some of these
are likely to apply to any garbage collector whatsoever.

## Questions and Answers

### I wrote a test program which allocates objects and registers finalizers for them.  Only a few (or no) objects are finalized.  What's wrong?

Probably nothing.  Finalizers are only executed if all of the following happen
before the process exits:

* A garbage collection runs.  This normally happens only after a significant
amount of allocation.
* The objects in question appear inaccessible at the time of the collection.
It is common for a handful of objects to appear accessible even though they
should not be, e.g. because temporary pointers to them haven't yet been
overwritten.  Also note that by default only the first item in a chain of
finalizable objects will be finalized in a collection.
* Another GC_ call notices that there are finalizers waiting to be run and
does so.

Small test programs typically don't run long enough for this to happen.

### Does this mean that the collector might leak memory?

In the short term yes.  But it is unlikely, though not impossible, that this
will result in a leak that grows over time.  Under normal circumstances, short
term, or one time leaks are a minor issue.  Memory leaks in explicitly managed
programs are feared because they almost always continue to grow over time.

For (a lot) more details see:

* "Bounding Space Usage of Conservative Garbage Collectors", Proceedings of
the 2002 ACM SIGPLAN-SIGACT Symposium on Principles of Programming Languages,
Jan. 2002, pp. 93-100
([official version](https://dl.acm.org/doi/10.1145/565816.503282),
[Technical report](http://www.hpl.hp.com/techreports/2001/HPL-2001-251.html)).

### How can I get more of the finalizers to run to convince myself that the GC is working?

Invoke GC_gcollect a couple of times just before process exit.

### I want to ensure that all my objects are finalized and reclaimed before process exit.  How can I do that?

You can't, and you do not really want that.  This would require finalizing
_reachable_ objects.  Finalizers run later would have to be able to handle
this, and would have to be able to run with randomly broken libraries, because
the objects they rely on where previously finalized.  In most environments,
you would also be replacing the operating systems mechanism for very
efficiently reclaiming process memory at process exit with a significantly
slower mechanism.

You do sometimes want to ensure that certain particular resources are
explicitly reclaimed before process exit, whether or not they become
unreachable.  Programming techniques for ensuring this are discussed in

* "Destructors, Finalizers, and Synchronization", Proceedings of the 2003 ACM
SIGPLAN-SIGACT Symposium on Principles of Programming Languages, Jan. 2003,
pp. 262-272
([official version](https://dl.acm.org/doi/10.1145/604131.604153),
[Technical report](http://www.hpl.hp.com/techreports/2002/HPL-2002-335.html),
[slides](http://www.hboehm.info/popl03/slides.pdf)).

### I wrote a memory allocation loop, and it runs much slower with the garbage collector than when I use malloc/free memory management.  Why?

Odds are your loop allocates very large objects and never initializes them.
Real programs generally don't behave that way.  Garbage collectors generally
perform appreciably worse for large object allocations, and they generally
initialize objects, even if you don't.

### What can I do to maximize allocation performance?

Here are some hints:

* Use `GC_MALLOC_ATOMIC` where possible.
* For a multi-threaded application, ensure the GC library is compiled with
`THREAD_LOCAL_ALLOC` macro defined (this is the default behavior) to avoid
locking on each allocation.
* If you use large statically allocated arrays or mapped files, consider
`GC_exclude_static_roots`.

### If my heap uses 2 GB on a 32-bit machine, won't every other integer or other random data be misinterpreted as a pointer by the collector?  Thus won't way too much memory be retained?

Maybe.  Probably, if the collector is used purely conservatively, with no
pointer layout information (such as use of `GC_MALLOC_ATOMIC`).

With a gigabyte heap, you are clearly much better off on a 64-bit machine.
Empirical evidence seems to suggest that some such applications work on
a 32-bit machine, and others don't perform acceptably.

Simple probability calculations for pointer misidentifications are generally
incorrect.  The probability of misinterpreting an integer is typically reduced
significantly by a number of collector features and fortunate accidents.  Most
integers are small, and small integers can generally not be heap addresses.
The collector black-listing mechanism avoids allocating areas that are prone
to be targets of misinterpreted references.  The collector can be told to
ignore some or all pointers to object interiors.

### I have a different question that is not answered here, nor in the other GC documentation.  Where else can I go?

If you can't find the answer in the [GC overview](overview.md) and the linked
files, please see "Feedback, Contribution, Questions and Notifications" section of
the main [README](../README.md).
