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
#include <functional>
#include <iostream>
#include <poet/active_function.hpp>
#include <vector>

int increment(int value)
{
// 	std::cerr << __FUNCTION__ << std::endl;
	// sleep for a bit to simulate doing something nontrivial
	boost::this_thread::sleep(boost::posix_time::millisec(100));
	return ++value;
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
	catch(const boost::signals2::expired_slot &)
	{
	}
	catch(...)
	{
		assert(false);
	}
}

void in_order_activation_queue_test()
{
	boost::shared_ptr<poet::in_order_activation_queue> activation_queue(new poet::in_order_activation_queue);
	boost::shared_ptr<poet::scheduler> scheduler(new poet::scheduler(activation_queue));
	poet::active_function<int (int)> inc(&increment, scheduler);
	std::vector<poet::promise<int> > promises;
	std::vector<poet::future<int> > results;

	promises.push_back(poet::promise<int>());
	results.push_back(inc(promises.back()));

	promises.push_back(poet::promise<int>());
	results.push_back(inc(promises.back()));

	promises.at(1).fulfill(1);

	assert(results.at(1).timed_join(boost::get_system_time() + boost::posix_time::milliseconds(100)) == false);
	assert(results.at(0).ready() == false);

	promises.at(0).fulfill(0);
	assert(results.at(0).get() == 1);
	assert(results.at(1).get() == 2);
}

void default_construction_test()
{
	poet::active_function<int (int, int)> default_constructed;
	poet::active_function<int (int, int)> adder((std::plus<int>()));
	default_constructed = adder;
	assert(default_constructed(1, 2).get() == 3);
}

int main()
{
	std::cerr << __FILE__ << "... ";

	{
		// the bind() is only for illustration and isn't actually needed in this case,
		// since the signatures match exactly.
		poet::active_function<int (int)> inc1(boost::bind(&increment, _1));
		poet::active_function<int (int)> inc2(&increment);
		std::vector<poet::future<int> > results(2);
		unsigned i;
		for(i = 0; i < results.size(); ++i)
		{
			poet::future<unsigned> temp = inc1(i);
			results.at(i) = inc2(temp);
		}
		for(i = 0; i < results.size(); ++i)
		{
			assert(results.at(i).get() == static_cast<int>(i + 2));
		}
	}
	slot_tracking_test();
	in_order_activation_queue_test();
	default_construction_test();

	std::cerr << "OK\n";
	return 0;
}
