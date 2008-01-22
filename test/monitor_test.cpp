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
#include <cmath>
#include <iostream>
#include <poet/monitor.hpp>
#include <poet/monitor_ptr.hpp>
#include <poet/monitor_base.hpp>
#include <unistd.h>
#include <utility>

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

class MyBase
{
public:
	virtual ~MyBase() {};
	virtual void set(int value) = 0;
	virtual int get() const = 0;
};

class MyDerived: public MyBase
{
public:
	MyDerived(): _value(0)
	{}
	virtual void set(int value)
	{
		_value = value;
	}
	virtual int get() const
	{
		return _value;
	}
private:
	int _value;
};

class MyDerivedToo: public MyBase
{
public:
	MyDerivedToo()
	{}
	virtual void set(int value)
	{
	}
	virtual int get() const
	{
		return 2;
	}
private:
};


void monitor_ptr_cast_test()
{
	static const int test_value = 2;
	poet::monitor_ptr<MyBase> mon_base(new MyDerived());
	mon_base->set(test_value);

	poet::monitor_ptr<MyDerived> mon_derived = poet::static_pointer_cast<MyDerived>(mon_base);
	assert(mon_derived->get() == test_value);
	mon_base = poet::static_pointer_cast<MyBase>(mon_derived);
	assert(mon_base->get() == test_value);

	mon_derived = poet::dynamic_pointer_cast<MyDerived>(mon_base);
	assert(mon_derived);
	assert(mon_derived->get() == test_value);
	mon_base = poet::dynamic_pointer_cast<MyBase>(mon_derived);
	assert(mon_base);
	assert(mon_base->get() == test_value);
	poet::monitor_ptr<MyDerivedToo> mon_derived_too = poet::dynamic_pointer_cast<MyDerivedToo>(mon_base);
	assert(mon_derived_too == 0);

	poet::monitor_ptr<const MyBase> mon_base_const = mon_base;
	assert(mon_base_const->get() == test_value);
	mon_base = poet::const_pointer_cast<MyBase>(mon_base);
	assert(mon_base->get() == test_value);
}

void monitor_ptr_test()
{
	std::cerr << __PRETTY_FUNCTION__;
	monitor_ptr_comparison_test();
	monitor_ptr_const_test();
	monitor_ptr_cast_test();
	step_counter = 0;
	monitor_ptr_type mymonitor(new Monitored);
	monitor_ptr_type mymonitor_too(mymonitor);
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

void monitor_construction_test()
{
	static const int first_test_value = -1;
	static const double second_test_value = 1.5;
	typedef poet::monitor<std::pair<int, double> > my_monitor_type;
	// construction of value by forwarding arguments to value constructor
	my_monitor_type mymon(first_test_value, second_test_value);
	{
		my_monitor_type::scoped_lock lock(mymon);
		assert(lock->first == first_test_value);
		assert(std::abs(lock->second - second_test_value) < 1e-6);
	}
	// copy-construction
	my_monitor_type mymon_too(mymon);
	{
		my_monitor_type::scoped_lock lock(mymon);
		my_monitor_type::scoped_lock lock_too(mymon_too);
		assert(lock_too->first == first_test_value);
		assert(std::abs(lock_too->second - second_test_value) < 1e-6);
	}

}

void monitor_assignment_test()
{
	static const int test_value = 3;
	poet::monitor<int> mymon;
	poet::monitor<int> mymon_too;
	mymon = test_value;
	{
		poet::monitor<int>::scoped_lock lock(mymon);
		assert(*lock == test_value);
	}
	mymon_too = mymon;
	{
		poet::monitor<int>::scoped_lock lock(mymon_too);
		assert(*lock == test_value);
	}
}

void monitor_test()
{
	std::cerr << __PRETTY_FUNCTION__;
	monitor_const_test();
	monitor_construction_test();
	monitor_assignment_test();
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
