// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <poet/active_function.hpp>
#include <iostream>
#include <vector>
#include <unistd.h>

static int delayed_value()
{
	sleep(1);
	return 1;
};

int main()
{
	static const unsigned nanosec_per_sec = 1000000000;

	poet::active_function<int ()> async_call(&delayed_value);
	poet::future<int> fut = async_call();
	boost::xtime now;
	boost::xtime_get(&now, boost::TIME_UTC);
	boost::xtime timeout = now;
	timeout.sec = timeout.sec + (timeout.nsec + 500000000) / nanosec_per_sec;
	timeout.nsec = (timeout.nsec + 500000000) % nanosec_per_sec;
	bool result = fut.timed_join(timeout);
	BOOST_ASSERT(result == false);
	timeout.sec = timeout.sec + 1;
	result = fut.timed_join(timeout);
	BOOST_ASSERT(result == true);
	return 0;
}
