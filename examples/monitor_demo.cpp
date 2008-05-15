// An example of using the monitor_ptr and monitor classes
// for automatically locked access to an object and waiting
// on the monitor's condition variable.

// Copyright (C) Frank Mori Hess 2007-2008
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


class Monitored: public poet::monitor_base
{
public:
	Monitored(): _state(0)
	{}
	void wait_for_state(int state)
	{
		std::cerr << "thread " << boost::this_thread::get_id() << ": " <<
			__FUNCTION__ << ": waiting for state = " << state << std::endl;
		while(_state != state) wait();
		std::cerr << "thread " << boost::this_thread::get_id() << ": " <<
			__FUNCTION__ << ": have state = " << state << std::endl;
	}
	void set_state(int state)
	{
		std::cerr << "thread " << boost::this_thread::get_id() << ": " <<
			__FUNCTION__ << ": state = " << state << std::endl;
		_state = state;
		notify_all();
	}
private:
	int _state;
};

//example of using poet::monitor_ptr

typedef poet::monitor_ptr<Monitored> monitor_ptr_type;

void monitor_ptr_thread0_function(monitor_ptr_type mymonitor)
{
	poet::monitor_unique_lock<monitor_ptr_type> mon_lock(mymonitor);
	mon_lock->set_state(1);
	mon_lock->wait_for_state(2);
	mon_lock->set_state(3);
	mon_lock->wait_for_state(4);
}

void monitor_ptr_thread1_function(monitor_ptr_type mymonitor)
{
	mymonitor->wait_for_state(1);
	{
		poet::monitor_unique_lock<monitor_ptr_type> mon_lock(mymonitor);
		mon_lock->set_state(2);
		mon_lock->wait_for_state(3);
	}
	mymonitor->set_state(4);
}

int main()
{
	typedef poet::monitor<Monitored> monitor_type;
	monitor_type mymonitor;
	boost::thread thread0(boost::bind(&monitor_ptr_thread0_function, mymonitor.get_monitor_ptr()));
	boost::thread thread1(boost::bind(&monitor_ptr_thread1_function, mymonitor.get_monitor_ptr()));
	thread0.join();
	thread1.join();

	return 0;
}
