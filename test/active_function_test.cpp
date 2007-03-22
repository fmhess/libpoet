/*
	A test program for futures and active object classes

	Author: Frank Hess <frank.hess@nist.gov>
	Begin: 2005-11
*/
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind.hpp>
#include <poet/active_function.hpp>
#include <iostream>
#include <vector>
#include <unistd.h>

int increment(int value)
{
// 	std::cerr << __FUNCTION__ << std::endl;
	// sleep for a bit to simulate doing something nontrivial
	sleep(1);
	return ++value;
}

bool black_knight()
{
	return false;
}

bool myGuard()
{
	return true;
}
int main()
{
	// the bind() is only for illustration and isn't actually needed in this case,
	// since the signatures match exactly.
	poet::active_function<int (int)> inc1(boost::bind(&increment, _1));
	poet::active_function<int (int)> inc2(&increment, &myGuard);
	std::vector<poet::future<int> > results(2);
	unsigned i;
	std::cerr << "getting Futures..." << std::endl;
	for(i = 0; i < results.size(); ++i)
	{
		poet::future<unsigned> temp = inc1(i);
		std::cerr << "passing result of inc1 to inc2..." << std::endl;
		results.at(i) = inc2(temp);
		std::cerr << "stored future results[" << i << "]" << std::endl;
	}
	std::cerr << "converting Futures to values..." << std::endl;
	for(i = 0; i < results.size(); ++i)
	{
		int value = results.at(i);
		std::cerr << "value from results[" << i << "] is " << value << std::endl;
	}

	// test cancellation
	boost::shared_ptr<poet::out_of_order_activation_queue> queue(new poet::out_of_order_activation_queue());
	boost::shared_ptr<poet::scheduler> sched(new poet::scheduler(-1, queue));
	poet::active_function<int (int)> cancelme(&increment, &black_knight, sched);
	poet::future<int> result = cancelme(0);
	BOOST_ASSERT(queue->empty() == false);
	BOOST_ASSERT(result.has_exception() == false);
	result.cancel();
	BOOST_ASSERT(result.has_exception() == true);
	sleep(1);
	BOOST_ASSERT(queue->empty() == true);

	return 0;
}
