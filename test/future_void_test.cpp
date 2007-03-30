// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <poet/future.hpp>
#include <iostream>
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
	std::cout << "Test passed." << std::endl;
	return 0;
}
