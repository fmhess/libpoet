/*
	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-08-27
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_BASE_IPP
#define _POET_MONITOR_BASE_IPP

#include <poet/monitor_base.hpp>

namespace poet
{
	void monitor_base::wait() const
	{
		_syncer->wait();
	}
	void monitor_base::notify_one() const
	{
		_syncer->notify_one();
	}
	void monitor_base::notify_all() const
	{
		_syncer->notify_all();
	}
	void monitor_base::set_synchronizer(const boost::shared_ptr<detail::monitor_synchronizer_base> &syncer) const
	{
		_syncer = syncer;
	}
};

#endif // _POET_MONITOR_BASE_IPP
