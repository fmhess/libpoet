// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/assert.hpp>
#include <boost/optional.hpp>
#include <iostream>
#include <poet/future.hpp>

void future_count_test()
{
	poet::promise<double> mypromise;
	BOOST_ASSERT(mypromise.has_future() == false);

	boost::optional<poet::future<double> > fut0(mypromise);
	BOOST_ASSERT(mypromise.has_future());

	{
		poet::future<int> fut1 = fut0.get();
		BOOST_ASSERT(mypromise.has_future());

		fut0.reset();
		BOOST_ASSERT(mypromise.has_future());
	}

	BOOST_ASSERT(mypromise.has_future() == false);
}

void promise_void_future_count_test()
{
	poet::promise<double> mypromise;
	poet::promise<void> void_promise = mypromise;
	BOOST_ASSERT(void_promise.has_future() == false);

	boost::optional<poet::future<double> > fut0(mypromise);
	BOOST_ASSERT(void_promise.has_future());

	fut0.reset();

	BOOST_ASSERT(void_promise.has_future() == false);
}

int main()
{
	std::cerr << __FILE__ << "... ";
	poet::future<double> myfuture;
	{
		poet::promise<double> mypromise_copy;
		{
			poet::promise<double> mypromise;
			myfuture = poet::future<double>(mypromise);
			BOOST_ASSERT(myfuture.has_exception() == false);
			mypromise_copy = mypromise;
		}
		BOOST_ASSERT(myfuture.has_exception() == false);
	}
	BOOST_ASSERT(myfuture.has_exception() == true);
	try
	{
		myfuture.get();
		BOOST_ASSERT(false);
	}
	catch(const poet::uncertain_future &)
	{}
	catch(...)
	{
		BOOST_ASSERT(false);
	}

	future_count_test();
	promise_void_future_count_test();

	std::cerr << "OK\n";
	return 0;
}
