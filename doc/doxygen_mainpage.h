/* This header exists purely for the sake of doxygen. */

/*! \mainpage libpoet Documentation
\section description Parallel Object Execution Threads
libpoet is a library whose goal is to make creation of active objects easy
enough for routine use.  Some of the more important classes in the library
are poet::active_function, poet::future, and poet::scheduler.

Active objects provide concurrency, since each active object executes in its own
thread.  Futures are employed to communicate with active objects
in a thread-safe manner.  To learn
more about the active object concept, see the paper
<a href="http://www.cse.wustl.edu/~doc/pspdfs/Act-Obj.pdf">"Active Object, An Object Behavioral Pattern for Concurrent Programming." by
R. Greg Lavender and Douglas C. Schmidt</a>.

A version of this documentation corresponding to the most recent release should
be available online at
<a href="http://www.comedi.org/projects/libpoet/index.html">http://www.comedi.org/projects/libpoet/index.html</a>

\section status Status

The library's API is still experimental, and subject to change as feedback
and experience clairifies areas which could be improved.

\section dependencies Dependencies

libpoet depends on the <a href="http://www.boost.org">boost C++ libraries</a> and
<a href="http://www.comedi.org/projects/thread_safe_signals/doxygen/index.html">thread_safe_signals</a>.
It requires linking to libboost_thread.

\section download Download

The most recent release is available from the
<a href="http://www.boost-consulting.com/vault/index.php?&direction=0&order=&directory=Concurrent%20Programming/Active%20Objects">
boost vault</a>
(get the "libpoet-xxxx-xx-xx.tgz" file).

The current source code is also availabe via anonymous cvs:
\code
cvs -d :pserver:anonymous@cvs.comedi.org:/cvs/comedi login
cvs -d :pserver:anonymous@cvs.comedi.org:/cvs/comedi co libpoet
\endcode

When prompted for a password, hit enter.

You can also
<a href="http://www.comedi.org/cgi-bin/viewcvs.cgi/libpoet/">browse the cvs online</a>.

The code is licensed under the <a href="http://www.boost.org/LICENSE_1_0.txt">Boost Software License, Version 1.0</a>.

\section installation Installation

libpoet is a header-only library.  It may be used by simply unpacking the files and adding the location of
the top-level directory (which contains the "poet/" subdirectory) to your compiler's include path.
Alternatively, you may move the "poet/" subdirectory
into an existing include directory which is already in your include path.

\section discussion Discussion

The <a href="http://www.boost.org/more/mailing_lists.htm">boost users and developers mailing lists</a>
may be used to discuss issues related to the library.

\author Frank Mori Hess <fmhess@users.sourceforge.net>


*/