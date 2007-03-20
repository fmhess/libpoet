// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <poet/future.hpp>
#include <iostream>
#include <vector>
#include <unistd.h>

static void delayed_increment(poet::promise<int> mypromise, poet::future<int> value)
{
	sleep(1);
	int intValue = value;
	++intValue;
	std::cerr << "Setting future value to " << intValue << std::endl;
	mypromise.fulfill(intValue);
};

static poet::future<int> async_delayed_increment(poet::future<int> value)
{
	poet::promise<int> mypromise;
	boost::thread mythread(boost::bind(&delayed_increment, mypromise, value));
	return mypromise;
}

int main()
{
	std::vector<poet::future<int> > futures;
	unsigned i;
	std::cerr << "getting Futures..." << std::endl;
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
	std::cerr << "converting Futures to values..." << std::endl;
	for(i = 0; i < 10; ++i)
	{
		int value = futures.at(i);
		std::cerr << "value from futures[" << i << "] is " << value << std::endl;
	}
	return 0;
}
