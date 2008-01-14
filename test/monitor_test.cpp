// An example of using the monitor_ptr and monitor classes
// for automatically locked access to an object.

// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <iostream>
#include <poet/monitor.hpp>
#include <poet/monitor_ptr.hpp>
#include <poet/monitor_base.hpp>
#include <unistd.h>

int step_counter;

void check_step(int expected)
{
	std::cerr << " " << expected;
	assert(step_counter++ == expected);
}

class Monitored: public poet::monitor_base
{
public:
	Monitored(): value(0)
	{}
	void waiting_function()
	{
		check_step(0);
		usleep(1000000);
		check_step(1);
		wait();
		check_step(4);
		usleep(1000000);
		check_step(5);
	}
	void notifying_function()
	{
		check_step(2);
		notify_all();
		usleep(1000000);
		check_step(3);
	}
	void another_function()
	{
		check_step(6);
		usleep(1000000);
		check_step(7);
	}
	int value;
};

//test for poet::monitor_ptr

typedef poet::monitor_ptr<Monitored> monitor_ptr_type;

void monitor_ptr_thread0_function(monitor_ptr_type mymonitor)
{
	mymonitor->waiting_function();
}

void monitor_ptr_thread1_function(monitor_ptr_type mymonitor)
{
	usleep(500000);
	mymonitor->notifying_function();
	usleep(500000);
	mymonitor->another_function();
}

void monitor_ptr_comparison_test()
{
	poet::monitor_ptr<char> mymon0(new char);
	poet::monitor_ptr<char> mymon0_copy = mymon0;
	poet::monitor_ptr<char> mymon1(new char);
	poet::monitor_ptr<char> mymon_null;
	assert(mymon_null == 0);
	assert(mymon_null != mymon0);
	assert(mymon_null != mymon1);
	assert(mymon0 != mymon1);
	assert(mymon0 == mymon0_copy);
	assert((mymon0 < mymon1 && !(mymon0 > mymon1)) || (mymon0 > mymon1 && !(mymon0 < mymon1)));
}

// make sure we can compile when using const monitor_ptr
void monitor_ptr_const_test()
{
	typedef poet::monitor_ptr<Monitored, boost::timed_mutex> mon_type;
	static const int test_value = 1;
	const mon_type mymon(new Monitored());
	mymon->value = test_value;
	assert(mymon->value == test_value);
	{
		const mon_type::scoped_lock lock(mymon);
		assert(lock->value == test_value);
	}
	{
		const mon_type::scoped_try_lock lock(mymon);
		assert(lock->value == test_value);
	}
	{
		const mon_type::scoped_timed_lock lock(mymon, true);
		assert(lock->value == test_value);
	}
}

void monitor_ptr_test()
{
	std::cerr << __PRETTY_FUNCTION__;
	monitor_ptr_comparison_test();
	monitor_ptr_const_test();
	step_counter = 0;
	monitor_ptr_type mymonitor(new Monitored);
	boost::thread thread0(boost::bind(&monitor_ptr_thread0_function, mymonitor));
	boost::thread thread1(boost::bind(&monitor_ptr_thread1_function, mymonitor));
	thread0.join();
	thread1.join();
	std::cerr << " OK" << std::endl;
}


// same thing done with poet::monitor

typedef poet::monitor<Monitored> monitor_type;

void monitor_thread0_function(monitor_type &mymonitor)
{
	monitor_type::scoped_lock mon_lock(mymonitor);
	mon_lock->waiting_function();
}

void monitor_thread1_function(monitor_type &mymonitor)
{
	usleep(500000);
	{
		monitor_type::scoped_lock mon_lock(mymonitor);
		mon_lock->notifying_function();
	}
	usleep(500000);
	{
		monitor_type::scoped_lock mon_lock(mymonitor);
		mon_lock->another_function();
	}
}

// make sure we can compile when using const monitor_ptr
void monitor_const_test()
{
	typedef poet::monitor<Monitored, boost::timed_mutex> mon_type;
	static const int test_value = 1;
	const mon_type mymon = Monitored();
	{
		const mon_type::scoped_lock lock(mymon);
		lock->value = test_value;
		assert(lock->value == test_value);
	}
	{
		const mon_type::scoped_try_lock lock(mymon);
		assert(lock->value == test_value);
	}
	{
		const mon_type::scoped_timed_lock lock(mymon, true);
		assert(lock->value == test_value);
	}
}

void monitor_test()
{
	std::cerr << __PRETTY_FUNCTION__;
	monitor_const_test();
	step_counter = 0;
	monitor_type mymonitor;
	boost::thread thread0(boost::bind(&monitor_thread0_function, boost::ref(mymonitor)));
	boost::thread thread1(boost::bind(&monitor_thread1_function, boost::ref(mymonitor)));
	thread0.join();
	thread1.join();
	std::cerr << " OK" << std::endl;
}

int main(int argc, const char **argv)
{
	monitor_ptr_test();
	monitor_test();
	return 0;
}
