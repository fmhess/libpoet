// Test program for ayclic_mutex

// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/shared_ptr.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
#include <poet/acyclic_mutex.hpp>

bool cycle_detected = false;

void cycle_handler()
{
	cycle_detected = true;
}

void shared_mutex_wrap_test()
{
	{
		poet::mutex_grapher::scoped_lock grapher;
		grapher->set_cycle_handler(&cycle_handler);
	}
	typedef poet::acyclic_mutex<boost::shared_mutex> mutex_type;
	mutex_type m;
	mutex_type n;
	{
		boost::shared_lock<mutex_type> shared(m);
		boost::upgrade_lock<mutex_type> upgrade(m);
		shared.unlock();
		boost::upgrade_to_unique_lock<mutex_type> unique(upgrade);
		assert(cycle_detected == false);
	}
	{
		boost::shared_lock<mutex_type> shared(m);
		boost::shared_lock<mutex_type> shared2(n);
		boost::shared_lock<mutex_type> shared3(m);
		assert(cycle_detected == false);
	}
	{
		boost::upgrade_lock<mutex_type> upgrade_m(m);
		boost::unique_lock<mutex_type> unique_n(n);
		assert(cycle_detected == false);
		boost::upgrade_to_unique_lock<mutex_type> unique_m(upgrade_m);
		assert(cycle_detected == true);
	}
}

int main()
{
	shared_mutex_wrap_test();
	std::cout << __FILE__ << ": OK" << std::endl;
	return 0;
}
