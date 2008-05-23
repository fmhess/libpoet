/*
	A test program for futures and active object classes

	Author: Frank Hess <frank.hess@nist.gov>
	Begin: 2005-11
*/
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <poet/active_function.hpp>
#include <iostream>
#include <vector>

int increment(int value)
{
// 	std::cerr << __FUNCTION__ << std::endl;
	// sleep for a bit to simulate doing something nontrivial
	boost::this_thread::sleep(boost::posix_time::seconds(1));
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

void cancellation_test()
{
	boost::shared_ptr<poet::out_of_order_activation_queue> queue(new poet::out_of_order_activation_queue());
	boost::shared_ptr<poet::scheduler> sched(new poet::scheduler(-1, queue));
	poet::active_function<int (int)> cancelme(&increment, &black_knight, sched);
	poet::future<int> result = cancelme(0);
	BOOST_ASSERT(queue->empty() == false);
	BOOST_ASSERT(result.has_exception() == false);
	result.cancel();
	BOOST_ASSERT(result.has_exception() == true);
	boost::this_thread::sleep(boost::posix_time::seconds(1));
	BOOST_ASSERT(queue->empty() == true);
}

class myclass
{
public:
	myclass(int value = 1): _value(value)
	{}
	int member_function()
	{
		return _value;
	}
private:
	int _value;
};

void slot_tracking_test()
{
	static const int test_value = 5;
	typedef poet::active_function<int ()> myfunc_type;
	boost::shared_ptr<myclass> myobj(new myclass(test_value));
	myfunc_type myfunc(myfunc_type::passive_slot_type(&myclass::member_function, myobj.get()).track(myobj));
	int retval = myfunc();
	BOOST_ASSERT(retval == test_value);
	myobj.reset();
	try
	{
		int retval;
		retval = myfunc();
		BOOST_ASSERT(false);
	}
	catch(const boost::expired_slot &err)
	{
		std::cout << "Caught expired_slot exception (good!): " << err.what() << std::endl;
	}
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

	cancellation_test();
	slot_tracking_test();

	return 0;
}
