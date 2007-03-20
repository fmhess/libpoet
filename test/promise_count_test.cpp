// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/assert.hpp>
#include <iostream>
#include <poet/future.hpp>

int main()
{
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
	catch(const poet::uncertain_future &err)
	{
		std::cerr << "caught exception: " << err.what() << std::endl;
	}
	catch(...)
	{
		BOOST_ASSERT(false);
	}
	return 0;
}
