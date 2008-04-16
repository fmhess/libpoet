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

struct cycle_detected_tag {};

void cycle_handler()
{
	cycle_detected = true;
	throw cycle_detected_tag();
}

void upgrade_test()
{
	{
		poet::mutex_grapher::scoped_lock grapher;
		grapher->set_cycle_handler(&cycle_handler);
	}
	typedef poet::acyclic_mutex<boost::shared_mutex> mutex_type;
	mutex_type m;
	{
		boost::shared_lock<mutex_type> shared(m);
		boost::upgrade_lock<mutex_type> upgrade(m);
		shared.unlock();
		{
			boost::upgrade_to_unique_lock<mutex_type> unique(upgrade);
			assert(cycle_detected == false);
		}
		try
		{
			boost::upgrade_lock<mutex_type> upgrade2(m);
			assert(false);
		}
		catch(const struct cycle_detected_tag &)
		{
		}
		catch(...)
		{
			assert(false);
		}
	}
}

int main()
{
	upgrade_test();
	std::cout << __FILE__ << ": OK" << std::endl;
	return 0;
}
