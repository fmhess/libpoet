// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <poet/active_function.hpp>
#include <iostream>
#include <vector>

static int delayed_value()
{
	boost::this_thread::sleep(boost::posix_time::seconds(1));
	return 1;
};

int main()
{
	poet::active_function<int ()> async_call(&delayed_value);
	poet::future<int> fut = async_call();
	boost::system_time timeout = boost::get_system_time() + boost::posix_time::millisec(500);
	bool result = fut.timed_join(timeout);
	BOOST_ASSERT(result == false);
	timeout = timeout + boost::posix_time::seconds(1);
	result = fut.timed_join(timeout);
	BOOST_ASSERT(result == true);
	std::cout << "Test passed." << std::endl;
	return 0;
}
