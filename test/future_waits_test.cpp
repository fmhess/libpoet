// Copyright (C) Frank Mori Hess 2007-2008
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <poet/future_waits.hpp>
#include <iostream>

void get_future(poet::future<void> f)
{
	f.get();
}

int main()
{
	std::cerr << __FILE__ << "... ";
	{
		poet::promise<double> x_promise;
		poet::future<double> x(x_promise);
		poet::promise<short> y_promise;
		poet::future<short> y(y_promise);
		poet::future<void> all_ready = poet::future_barrier(x, y);
		assert(all_ready.ready() == false);
		assert(all_ready.has_exception() == false);
		boost::thread blocking_thread(boost::bind(&get_future, all_ready));
		boost::this_thread::sleep(boost::posix_time::millisec(200));
		x_promise.fulfill(1.0);
		assert(all_ready.ready() == false);
		assert(all_ready.has_exception() == false);
		bool thread_done = blocking_thread.timed_join(boost::get_system_time());
		assert(thread_done == false);
		y_promise.fulfill(2);
		assert(all_ready.ready() == true);
		assert(all_ready.has_exception() == false);
		blocking_thread.join();
	}

	{
		poet::promise<short> x_promise;
		poet::future<short> x(x_promise);
		poet::promise<short> y_promise;
		poet::future<short> y(y_promise);
		poet::future<short> any_ready = poet::future_select(x, y);
		assert(any_ready.ready() == false);
		assert(any_ready.has_exception() == false);
		boost::thread blocking_thread(boost::bind(&get_future, any_ready));
		boost::this_thread::sleep(boost::posix_time::millisec(200));
		bool thread_done = blocking_thread.timed_join(boost::get_system_time());
		assert(thread_done == false);
		static const short x_value = 1;
		x_promise.fulfill(x_value);
		assert(any_ready.ready() == true);
		assert(any_ready.has_exception() == false);
		blocking_thread.join();
		y_promise.fulfill(2);
		assert(any_ready.ready() == true);
		assert(any_ready.has_exception() == false);
		assert(any_ready.get() == x_value);
	}
	
	{
		poet::promise<short> x_promise;
		poet::future<short> x(x_promise);
		poet::promise<double> y_promise;
		poet::future<double> y(y_promise);
		poet::future<void> any_ready = poet::future_select<void>(x, y);
		assert(any_ready.ready() == false);
		assert(any_ready.has_exception() == false);
		boost::thread blocking_thread(boost::bind(&get_future, any_ready));
		boost::this_thread::sleep(boost::posix_time::millisec(200));
		bool thread_done = blocking_thread.timed_join(boost::get_system_time());
		assert(thread_done == false);
		static const short x_value = 1;
		x_promise.fulfill(x_value);
		assert(any_ready.ready() == true);
		assert(any_ready.has_exception() == false);
		blocking_thread.join();
		y_promise.fulfill(2.0);
		assert(any_ready.ready() == true);
		assert(any_ready.has_exception() == false);
	}
	std::cerr << "OK\n";
	return 0;
}
