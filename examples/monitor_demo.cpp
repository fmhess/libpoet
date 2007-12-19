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


class Monitored: public poet::monitor_base
{
public:
	void waiting_function()
	{
		std::cerr << __FUNCTION__ << ": enter (mutex acquired)" << std::endl;
		usleep(1000000);
		std::cerr << __FUNCTION__ << ": waiting... (mutex released)" << std::endl;
		wait();
		std::cerr << __FUNCTION__ << ": wait complete (mutex acquired)" << std::endl;
		usleep(1000000);
		std::cerr << __FUNCTION__ << ": exit (mutex released)" << std::endl;
	}
	void notifying_function()
	{
		std::cerr << __FUNCTION__ << ": enter, notifying (mutex acquired)" << std::endl;
		notify_all();
		usleep(1000000);
		std::cerr << __FUNCTION__ << ": exit (mutex released)" << std::endl;
	}
	void another_function()
	{
		std::cerr << __FUNCTION__ << ": enter (mutex acquired)" << std::endl;
		usleep(1000000);
		std::cerr << __FUNCTION__ << ": exit (mutex released)" << std::endl;
	}
};

//example of using poet::monitor_ptr

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

void monitor_ptr_example()
{
	std::cerr << "\n" << __PRETTY_FUNCTION__ << std::endl;
	monitor_ptr_type mymonitor(new Monitored);
	boost::thread thread0(boost::bind(&monitor_ptr_thread0_function, mymonitor));
	boost::thread thread1(boost::bind(&monitor_ptr_thread1_function, mymonitor));
	thread0.join();
	thread1.join();
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

void monitor_example()
{
	std::cerr << "\n" << __PRETTY_FUNCTION__ << std::endl;
	monitor_type mymonitor;
	boost::thread thread0(boost::bind(&monitor_thread0_function, boost::ref(mymonitor)));
	boost::thread thread1(boost::bind(&monitor_thread1_function, boost::ref(mymonitor)));
	thread0.join();
	thread1.join();
}

int main()
{
	monitor_ptr_example();
	monitor_example();
	return 0;
}
