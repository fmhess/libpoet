// Some tests for new mutex api based on Boost.Thread 1.35.0

// Copyright (C) Frank Mori Hess 2008
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/thread.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <poet/monitor.hpp>
#include <poet/monitor_ptr.hpp>

struct point
{
	point(int init_x = 0, int init_y = 0):
		x(init_x), y(init_y)
	{}
	void const_function() const {};
	void nonconst_function() {};
	int x;
	int y;
};

void monitor_unique_lock_test()
{
	static const int test_value = 5;
	{
		typedef poet::monitor_ptr<int> monitor_type;
		monitor_type mon(new int(test_value));
		poet::monitor_unique_lock<monitor_type> lock(mon);
		assert(*lock == test_value);
	}
	{
		typedef poet::monitor_ptr<point> monitor_type;
		monitor_type mon(new point(test_value));
		poet::monitor_unique_lock<monitor_type> lock(mon);
		assert(lock->x == test_value);
	}
	{
		typedef poet::monitor<int> monitor_type;
		monitor_type mon(test_value);
		poet::monitor_unique_lock<monitor_type> lock(mon);
		assert(*lock == test_value);
	}
	{
		typedef poet::monitor<point> monitor_type;
		monitor_type mon = point(test_value);
		poet::monitor_unique_lock<monitor_type> lock(mon);
		assert(lock->x == test_value);
	}
#if 0
// the following should not compile, due to violating constness
	{
		typedef poet::monitor_ptr<const point> monitor_type;
		monitor_type mon(new point(test_value));
		poet::monitor_unique_lock<monitor_type> lock(mon);
		lock->nonconst_function();
	}
#endif
	{
		typedef poet::monitor_ptr<const point> monitor_type;
		monitor_type mon(new point(test_value));
		poet::monitor_unique_lock<monitor_type> lock(mon);
		lock->const_function();
	}
#if 0
// the following should not compile, due to violating constness
	{
		typedef const poet::monitor<point> monitor_type;
		monitor_type mon = point(test_value);
		poet::monitor_unique_lock<monitor_type> lock(mon);
		lock->nonconst_function();
	}
#endif
	{
		typedef const poet::monitor<point> monitor_type;
		monitor_type mon = point(test_value);
		poet::monitor_unique_lock<monitor_type> lock(mon);
		lock->const_function();
	}
#if 0
// the following should not compile, due to violating constness
	{
		typedef poet::monitor<const point> monitor_type;
		monitor_type mon = point(test_value);
		poet::monitor_unique_lock<monitor_type> lock(mon);
		lock->nonconst_function();
	}
#endif
	{
		typedef poet::monitor<const point> monitor_type;
		monitor_type mon = point(test_value);
		poet::monitor_unique_lock<monitor_type> lock(mon);
		lock->const_function();
	}
}

void monitor_rw_lock_test()
{
	static const int test_value = 3;
	{
		typedef poet::monitor<point, boost::shared_mutex> monitor_type;
		monitor_type mon = point(test_value);
		poet::monitor_shared_lock<monitor_type> shared(mon);
		assert(shared->x == test_value);
		shared->const_function();
		poet::monitor_upgrade_lock<monitor_type> upgrade(mon);
		shared.unlock();
		poet::monitor_upgrade_to_unique_lock<monitor_type> unique(upgrade);
		unique->const_function();
		unique->nonconst_function();
	}
}

// monitor and monitor_ptr should be useable with Boost.Thread locks
void lockable_concept_test()
{
	{
		typedef poet::monitor_ptr<int> monitor_type;
		monitor_type mon(new int(0));
		{
			boost::lock_guard<monitor_type> lock(mon);
		}
		{
			boost::unique_lock<monitor_type> lock(mon);
		}
	}
	{
		typedef poet::monitor_ptr<int, boost::shared_mutex> monitor_type;
		monitor_type mon(new int(0));
		{
			boost::shared_lock<monitor_type> shared(mon);
			boost::upgrade_lock<monitor_type> upgrade(mon);
			shared.unlock();
			boost::upgrade_to_unique_lock<monitor_type> unique(upgrade);
		}
	}
	{
		typedef poet::monitor<int> monitor_type;
		monitor_type mon(0);
		{
			boost::lock_guard<monitor_type> lock(mon);
		}
		{
			boost::unique_lock<monitor_type> lock(mon);
		}
	}
	{
		typedef poet::monitor<int, boost::shared_mutex> monitor_type;
		monitor_type mon(0);
		{
			boost::shared_lock<monitor_type> shared(mon);
			boost::upgrade_lock<monitor_type> upgrade(mon);
			shared.unlock();
			boost::upgrade_to_unique_lock<monitor_type> unique(upgrade);
		}
	}
}

int main(int argc, const char **argv)
{
	monitor_unique_lock_test();
	monitor_rw_lock_test();
	lockable_concept_test();
	return 0;
}
