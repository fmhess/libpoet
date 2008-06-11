// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <cassert>
#include <poet/detail/identity.hpp>
#include <poet/future_barrier.hpp>
#include <poet/future.hpp>
#include <iostream>
#include <vector>

static void delayed_increment(poet::promise<int> mypromise, poet::future<int> value)
{
	boost::this_thread::sleep(boost::posix_time::millisec(100));
	int intValue = value;
	++intValue;
	mypromise.fulfill(intValue);
};

static poet::future<int> async_delayed_increment(poet::future<int> value)
{
	poet::promise<int> mypromise;
	boost::thread mythread(boost::bind(&delayed_increment, mypromise, value));
	return mypromise;
}

// fulfill a promise with a future that is already ready
void promise_fulfill_ready_future_test()
{
	static const int test_value = 4;
	poet::promise<int> mypromise;
	poet::future<int> myfuture = mypromise;
	poet::future<int> fulfilling_future = test_value;
	mypromise.fulfill(fulfilling_future);
	assert(myfuture.ready());
	assert(myfuture.get() == test_value);
}

// fulfill a promise with a lazy future that must be waited on to become ready
void promise_fulfill_lazy_future_test()
{
	{
		static const int test_value = 3;
		poet::promise<int> mypromise;
		poet::future<int> myfuture = mypromise;
		poet::promise<int> dependency_promise;
		poet::future<int> dependency = dependency_promise;
		poet::future<int> lazy_fulfilling_future = poet::future_combining_barrier<int>(
			poet::detail::identity(), poet::detail::identity(), dependency);
		mypromise.fulfill(lazy_fulfilling_future);
		dependency_promise.fulfill(test_value);
		assert(myfuture.ready());
		assert(myfuture.get() == test_value);
	}
	// now test promise<void> specialization
	{
		static const int test_value = 3;
		poet::promise<void> mypromise;
		poet::future<void> myfuture = mypromise;
		poet::promise<int> dependency_promise;
		poet::future<int> dependency = dependency_promise;
		poet::future<int> lazy_fulfilling_future = poet::future_combining_barrier<int>(
			poet::detail::identity(), poet::detail::identity(), dependency);
		mypromise.fulfill(lazy_fulfilling_future);
		dependency_promise.fulfill(test_value);
		assert(myfuture.ready());
		myfuture.get();
	}
}

int main()
{
	std::cerr << __FILE__ << "... ";

	std::vector<poet::future<int> > futures;
	unsigned i;
	for(i = 0; i < 10; ++i)
	{
		if(i == 0)
		{
			futures.push_back(async_delayed_increment(0));
		}else
		{
			futures.push_back(async_delayed_increment(futures.at(i - 1)));
		}
	}
	for(i = 0; i < 10; ++i)
	{
		assert(futures.at(i).get() == static_cast<int>(i + 1));
	}

	promise_fulfill_ready_future_test();
	promise_fulfill_lazy_future_test();

	std::cerr << "OK\n";
	return 0;
}
