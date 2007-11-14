/*
	By deriving from monitor_base, an object can release the
	mutex of the monitor_ptr which owns it and wait to be
	notified.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-08-27
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <poet/detail/monitor_base_decl.hpp>
#include <poet/detail/monitor_base.ipp>

