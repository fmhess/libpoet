// Some tests for new mutex api based on Boost.Thread 1.35.0

// Copyright (C) Frank Mori Hess 2008
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <poet/acyclic_mutex.hpp>
#include <poet/monitor.hpp>
#include <poet/monitor_ptr.hpp>
#include <poet/monitor_base.hpp>

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
	{
		try
		{
			typedef poet::monitor_ptr<int> monitor_type;
			monitor_type mon;
			// locking an empty monitor_ptr throws
			poet::monitor_unique_lock<monitor_type> lock(mon);
			assert(false);
		}
		catch(const boost::lock_error &)
		{}
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

template<typename Lock1, typename Lock2, typename Monitor>
void monitor_lock_move_test(Monitor &mon)
{
	{
		Lock1 lock1(mon);
		Lock2 lock2(mon, boost::defer_lock_t());
		assert(lock1.owns_lock());
		assert(lock2.owns_lock() == false);
		lock2 = lock1.move();
		assert(lock2.owns_lock());
		assert(lock2.mutex() == &mon);
	}
	{
		Lock1 lock1(mon);
		assert(lock1.owns_lock());
		Lock2 lock2(move(lock1));
		assert(lock1.owns_lock() == false);
		assert(lock2.owns_lock());
		assert(lock2.mutex() == &mon);
	}
}

template<typename Lock, typename Monitor>
void monitor_temp_lock_move_test(Monitor &mon)
{
	Lock lock = Lock(mon);
	assert(lock.owns_lock());
}

template<typename Lock, typename Monitor>
void monitor_swap_test()
{
	Monitor mon1;
	Lock lock1(mon1);
	Monitor mon2;
	Lock lock2(mon2);

	using std::swap;
	swap(lock1, lock2);
	assert(lock1.owns_lock());
	assert(lock1.mutex() == &mon2);
	assert(lock2.owns_lock());
	assert(lock2.mutex() == &mon1);

	lock1.swap(lock2);
	assert(lock1.owns_lock());
	assert(lock1.mutex() == &mon1);
	assert(lock2.owns_lock());
	assert(lock2.mutex() == &mon2);
}

void upgrade_to_unique_move_test()
{
	typedef poet::monitor<int, boost::shared_mutex> monitor_type;
	monitor_type mon1;
	monitor_type mon2;
	{
		poet::monitor_upgrade_lock<monitor_type> upgrade1(mon1);
		poet::monitor_upgrade_to_unique_lock<monitor_type> lock1(upgrade1);
		assert(lock1.owns_lock());
		poet::monitor_upgrade_lock<monitor_type> upgrade2(mon2);
		poet::monitor_upgrade_to_unique_lock<monitor_type> lock2(upgrade2);
		lock2 = move(lock1);
		assert(lock1.owns_lock() == false);
		assert(lock2.owns_lock());
	}
	{
		poet::monitor_upgrade_lock<monitor_type> upgrade1(mon1);
		poet::monitor_upgrade_to_unique_lock<monitor_type> lock1(upgrade1);
		assert(lock1.owns_lock());
		poet::monitor_upgrade_to_unique_lock<monitor_type> lock2(lock1.move());
		assert(lock1.owns_lock() == false);
		assert(lock2.owns_lock());
	}
	{
		poet::monitor_upgrade_lock<monitor_type> upgrade1(mon1);
		poet::monitor_upgrade_to_unique_lock<monitor_type> lock1(upgrade1);
		assert(lock1.owns_lock());
		poet::monitor_upgrade_lock<monitor_type> upgrade2(mon2);
		poet::monitor_upgrade_to_unique_lock<monitor_type> lock2(upgrade2);
		assert(lock2.owns_lock());
		using std::swap;
		swap(lock1, lock2);
		assert(lock1.owns_lock());
		assert(lock2.owns_lock());
	}
}

void double_move_test()
{
	typedef poet::monitor<int, boost::shared_mutex> monitor_type;
	monitor_type mon1;
	poet::monitor_unique_lock<monitor_type> lock1(mon1);
	monitor_type mon2;
	poet::monitor_unique_lock<monitor_type> lock2(mon2);
	lock2 = move(move(lock1));
	assert(lock1.owns_lock() == false);
	assert(lock2.owns_lock() == true);
	assert(lock2.mutex() == &mon1);
}

void monitor_lock_move_tests()
{
	std::cerr << __FUNCTION__ << "... ";
	{
		typedef poet::monitor_ptr<int> monitor_type;
		monitor_type mon(new int(0));
		monitor_lock_move_test<poet::monitor_unique_lock<monitor_type>, poet::monitor_unique_lock<monitor_type> >(mon);
	}
	{
		typedef poet::monitor_ptr<int, boost::shared_mutex> monitor_type;
		monitor_type mon(new int(0));
		monitor_lock_move_test<poet::monitor_upgrade_lock<monitor_type>, poet::monitor_unique_lock<monitor_type> >(mon);
		// moves into upgrade_lock
		monitor_lock_move_test<poet::monitor_unique_lock<monitor_type>, poet::monitor_upgrade_lock<monitor_type> >(mon);
		monitor_lock_move_test<poet::monitor_upgrade_lock<monitor_type>, poet::monitor_upgrade_lock<monitor_type> >(mon);
		// moves into shared_lock
		monitor_lock_move_test<poet::monitor_unique_lock<monitor_type>, poet::monitor_shared_lock<monitor_type> >(mon);
		monitor_lock_move_test<poet::monitor_upgrade_lock<monitor_type>, poet::monitor_shared_lock<monitor_type> >(mon);
		monitor_lock_move_test<poet::monitor_shared_lock<monitor_type>, poet::monitor_shared_lock<monitor_type> >(mon);
		// moves from temporary
		monitor_temp_lock_move_test<poet::monitor_unique_lock<monitor_type> >(mon);
		monitor_temp_lock_move_test<poet::monitor_upgrade_lock<monitor_type> >(mon);
		monitor_temp_lock_move_test<poet::monitor_shared_lock<monitor_type> >(mon);
	}
	{
		monitor_swap_test<poet::monitor_unique_lock<poet::monitor<int> >, poet::monitor<int> >();
		monitor_swap_test<poet::monitor_upgrade_lock<poet::monitor<int, boost::shared_mutex> >, poet::monitor<int, boost::shared_mutex> >();
		monitor_swap_test<poet::monitor_shared_lock<poet::monitor<int, boost::shared_mutex> >, poet::monitor<int, boost::shared_mutex> >();
	}
	upgrade_to_unique_move_test();
	double_move_test();
	std::cerr << "OK" << std::endl;
}

void lock_release_test()
{
	typedef poet::monitor_ptr<int> monitor_type;
	monitor_type mon(new int(0));
	poet::monitor_unique_lock<monitor_type> lock(mon);
	assert(lock.owns_lock() == true);
	assert(lock.mutex() == &mon);
	monitor_type *result = lock.release();
	assert(result == &mon);
	assert(lock.owns_lock() == false);
	assert(lock.mutex() == 0);
	poet::monitor_unique_lock<monitor_type> lock2(mon, boost::adopt_lock_t());
	assert(lock2.owns_lock() == true);
	assert(lock2.mutex() == &mon);
	poet::monitor_unique_lock<monitor_type> lock3(mon, boost::defer_lock_t());
	assert(lock3.try_lock() == false);
}

class waiting_class: public poet::monitor_base
{
public:
	waiting_class(): _notified(false), _waiting(false)
	{}
	void do_wait()
	{
		while(_notified == false)
		{
			_waiting = true;
			wait();
			_waiting = false;
		}
	}
	void do_notify()
	{
		_notified = true;
		notify_all();
	}
	bool waiting() const {return _waiting;}
private:
	bool _notified;
	bool _waiting;
};

typedef poet::monitor_ptr<waiting_class, poet::acyclic_mutex<boost::mutex> > acyclic_monitor_type;

void acyclic_monitor_thread_func(acyclic_monitor_type mon)
{
	mon->do_wait();
}

void acyclic_monitor_test()
{
	/* new boost.thread implementation should allow non-boost.thread mutexes to
	be used with conditions .*/
	acyclic_monitor_type mon(new waiting_class);
	boost::thread mythread(boost::bind(&acyclic_monitor_thread_func, mon));
	while(mon->waiting() == false) boost::this_thread::sleep(boost::posix_time::millisec(10));
	mon->do_notify();
	mythread.join();
}

int main(int argc, const char **argv)
{
	monitor_unique_lock_test();
	monitor_rw_lock_test();
	lockable_concept_test();
	monitor_lock_move_tests();
	lock_release_test();
	acyclic_monitor_test();

	return 0;
}
