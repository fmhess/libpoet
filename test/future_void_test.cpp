// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <poet/future.hpp>
#include <iostream>
#include <stdexcept>

int main()
{
	{
		poet::future<double> fut(5.);
		poet::future<void> void_fut = fut;
		BOOST_ASSERT(void_fut.ready());
		BOOST_ASSERT(void_fut.has_exception() == false);
	}
	{
		poet::future<double> uncertain_fut;
		poet::future<void> void_uncertain_fut = uncertain_fut;
		BOOST_ASSERT(void_uncertain_fut.ready() == false);
		BOOST_ASSERT(void_uncertain_fut.has_exception());
	}
	{
		poet::promise<int> mypromise;
		poet::promise<void> void_promise = mypromise;
		poet::promise<void> void_promise_too = void_promise;
		poet::future<int> fut(mypromise);
		BOOST_ASSERT(fut.ready() == false);
		BOOST_ASSERT(fut.has_exception() == false);
		try
		{
			void_promise.fulfill();
			BOOST_ASSERT(false);
		}
		catch(const std::invalid_argument &)
		{}
		void_promise_too.renege(std::runtime_error("reneged on promise"));
		BOOST_ASSERT(fut.ready() == false);
		BOOST_ASSERT(fut.has_exception() == true);
	}
	std::cout << "Test passed." << std::endl;
	return 0;
}
