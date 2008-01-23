// Test program for ayclic_mutex

// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/shared_ptr.hpp>
#include <cassert>
#include <fstream>
#include <iostream>
#include <poet/acyclic_mutex.hpp>

typedef poet::acyclic_mutex<boost::mutex> mutex_type;

bool cycle_detected = false;

void cycle_handler()
{
	cycle_detected = true;
}

int main()
{
	{
		poet::mutex_grapher::scoped_lock grapher;
		grapher->set_cycle_handler(&cycle_handler);
	}
	mutex_type mutex_a("a");
	mutex_type mutex_b("b");
	boost::shared_ptr<mutex_type> mutex_x(new mutex_type);
	{
		mutex_type::scoped_lock lock_a(mutex_a);
		mutex_type::scoped_lock lock_x(*mutex_x);
	}
	{
		mutex_type::scoped_lock lock_x(*mutex_x);
		mutex_type::scoped_lock lock_b(mutex_b);
	}
	{
		/* deleting the mutex and creating a new one should not
		change the number of nodes in the mutex graph, since the
		node for the deleted mutex should be reused. */
		poet::mutex_grapher::scoped_lock grapher;
		const unsigned num_nodes = boost::num_vertices(grapher->graph());
		assert(num_nodes > 0);
		grapher.unlock();

		mutex_x.reset(new mutex_type);
		grapher.lock();
		assert(num_nodes == boost::num_vertices(grapher->graph()));
	}
	{
		// this doesn't violate locking order, because we reset mutex_x with a new mutex
		mutex_type::scoped_lock lock_b(mutex_b);
		mutex_type::scoped_lock lock_x(*mutex_x);
		assert(cycle_detected == false);
	}
	{	/* this will violate the locking order we have established,
			and will result in the cycle handler being called */
		mutex_type::scoped_lock lock_x(*mutex_x);
		mutex_type::scoped_lock lock_a(mutex_a);
		assert(cycle_detected);
	}
	std::cout << __FILE__ << ": OK" << std::endl;
	return 0;
}
