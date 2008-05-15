// An example of using ayclic_mutex to debug mutex
// locking order violations.

// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/thread/locks.hpp>
#include <cstdlib>
#include <poet/acyclic_mutex.hpp>
#include <iostream>

typedef poet::acyclic_mutex<boost::mutex> mutex_type;

/* dumps mutexes locked by current thread to stderr, and
a graphviz file to stdout. */
void cycle_handler()
{
	poet::mutex_grapher::unique_lock grapher;
	std::cerr << "Detected cycle in locking order graph.\n"
		<< "Currently locked mutexes:";
	poet::mutex_grapher::mutex_list_type::const_iterator it;
	for(it = grapher->locked_mutexes().begin();
		it != grapher->locked_mutexes().end(); ++it)
	{
		std::cerr << "\n\tname = \"" << grapher->graph()[(*it)->vertex().get()].name << "\"";
	}
	std::cerr << std::endl;
	grapher->write_graphviz(std::cout);
	exit(1);
}

int main()
{
	// replace the default cycle handler (which aborts the program)
	{
		poet::mutex_grapher::unique_lock grapher;
		grapher->set_cycle_handler(&cycle_handler);
	}

	mutex_type mutex_a("a");
	mutex_type mutex_b("b");
	mutex_type mutex_x;
	{
		boost::unique_lock<mutex_type> lock_a(mutex_a);
		boost::unique_lock<mutex_type> lock_x(mutex_x);
	}
	{
		boost::unique_lock<mutex_type> lock_x(mutex_x);
		boost::unique_lock<mutex_type> lock_b(mutex_b);
		/* this will violate the locking order we have established,
			and will result in the cycle handler being called */
		boost::unique_lock<mutex_type> lock_a(mutex_a);
	}
	return 0;
}
