/*
	Extensions of Boost.Thread lock classes, which add support for
	operator->() and operator*().  Used in conjunction with
	monitor_ptr and monitor classes.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-08-27
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_LOCKS_HPP
#define _POET_MONITOR_LOCKS_HPP

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/exceptions.hpp>
#include <boost/thread/locks.hpp>
#include <poet/detail/monitor_synchronizer.hpp>
#include <poet/monitor_ptr.hpp>

namespace poet
{
	template<typename T, typename Mutex>
	class monitor_ptr;

	template<typename Monitor>
	class monitor_unique_lock
	{
	public:
		typedef typename Monitor::element_type element_type;

		monitor_unique_lock(const Monitor &mon):
			_mon(mon), _lock(_mon)
		{
			set_wait_function();
		}
		template<typename T>
		monitor_unique_lock(const Monitor &mon, const T &arg):
			_mon(mon), _lock(_mon, arg)
		{
			set_wait_function();
		}

		// unique_lock interface
		void swap(monitor_unique_lock &other)
		{
			poet::swap(_mon, other._mon);
			_lock.swap(other._lock);
			set_wait_function();
			other.set_wait_function();
		}
		void lock()
		{
			_lock.lock();
			set_wait_function();
		}
		bool try_lock()
		{
			bool successful = _lock.try_lock();
			set_wait_function();
			return successful;
		}
		template<typename Timeout>
		bool timed_lock(const Timeout &timeout)
		{
			bool successful = _lock.timed_lock(timeout);
			set_wait_function();
			return successful;
		}
		void unlock()
		{
			_lock.unlock();
		}
		bool owns_lock() const
		{
			return _lock.owns_lock();
		}
		bool locked() const // backwards compatibility
		{
			return owns_lock();
		}
		// safe bool idiom, somewhat safer than providing conversion to bool operator
		typedef boost::unique_lock<Monitor> * ::poet::monitor_unique_lock<Monitor>::* unspecified_bool_type;
		operator unspecified_bool_type() const
		{
			return !_lock ? 0 : &_lock;
		}
		bool operator!() const
		{
			return !_lock;
		}
		Monitor* mutex() const
		{
			return _lock.mutex();
		}
		Monitor* release()
		{
			return _lock.release();
		}

		// extensions to unique_lock interface
		element_type* operator->() const
		{
			if(owns_lock() == false)
			{
				throw boost::lock_error();
			}
			return _mon.direct().get();
		}
		element_type& operator*() const
		{
			if(owns_lock() == false)
			{
				throw boost::lock_error();
			}
			return *_mon.direct().get();
		}

	private:
		void set_wait_function()
		{
			if(_lock.owns_lock())
			{
				typename detail::monitor_synchronizer<typename Monitor::mutex_type>::wait_function_type wait_func = boost::bind(
					&poet::monitor_unique_lock<Monitor>::wait_function, this, _1, _2);
				_mon._syncer->set_wait_function(wait_func);
			}
		}

		void wait_function(boost::condition &condition, const boost::function<bool ()> &pred)
		{
			if(pred == 0)
				condition.wait(_lock);
			else
				condition.wait(_lock, pred);
		}
		monitor_ptr<typename Monitor::element_type, typename Monitor::mutex_type> _mon;
		boost::unique_lock<Monitor> _lock;
	};
};

#endif // _POET_MONITOR_LOCKS_HPP
