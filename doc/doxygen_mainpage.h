/* This header exists purely for the sake of doxygen. */

/*! \mainpage libpoet Documentation
\section description Parallel Object Execution Threads
libpoet is a framework intended to make creation of active objects easy
enough for routine use.  Some of the more important classes in the library
are poet::future, poet::active_function, and poet::scheduler.  To learn
more about active objects, see the paper
<a href="http://www.cse.wustl.edu/~doc/pspdfs/Act-Obj.pdf">"Active Object, An Object Behavioral Pattern for Concurrent Programming." by
R. Greg Lavender and Douglas C. Schmidt</a>.

\section status Status

The library's API is still experimental, and subject to change as feedback
and experience clairifies areas which could be improved.

\section dependencies Dependencies

libpoet depends on the <a href="http://www.boost.org">boost C++ libraries</a> and
<a href="http://www.comedi.org/projects/thread_safe_signals/doxygen/index.html">thread_safe_signals</a>.

\section download Download

The current source code is availabe via anonymous cvs:
\code
cvs -d :pserver:anonymous@cvs.comedi.org:/cvs/comedi login
cvs -d :pserver:anonymous@cvs.comedi.org:/cvs/comedi co libpoet
\endcode

When prompted for a password, hit enter.

You can also
<a href="http://www.comedi.org/cgi-bin/viewcvs.cgi/libpoet/">browse the cvs online</a>.

The code is licensed under the <a href="http://www.boost.org/LICENSE_1_0.txt">Boost Software License, Version 1.0</a>.

\section discussion Discussion

The <a href="http://www.boost.org/more/mailing_lists.htm">boost users and developers mailing lists</a>
may be used to discuss issues related to the library.

\author Frank Mori Hess <fmhess@users.sourceforge.net>


*/