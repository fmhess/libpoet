/*
	An experimental alternative to monitor_ptr that stores the wrapped
	object by value instead of acting like a pointer.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-11-29
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_HPP
#define _POET_MONITOR_HPP

#include <boost/scoped_ptr.hpp>
#include <poet/detail/monitor_locks.hpp>
#include <poet/monitor_ptr.hpp>

namespace poet
{
	template<typename T, typename Mutex>
	class try_monitor;
	template<typename T, typename Mutex>
	class timed_monitor;

	template<typename T, typename Mutex = boost::mutex>
	class monitor
	{
	public:
		typedef T value_type;
		typedef Mutex mutex_type;

		class scoped_lock: public detail::monitor_scoped_lock<T, Mutex>
		{
			typedef typename detail::monitor_scoped_lock<T, Mutex> base_class;
		public:
			scoped_lock(monitor<T, Mutex> &mon): base_class(mon._monitor_pointer)
			{}
			scoped_lock(monitor<T, Mutex> &mon, bool do_lock):
				base_class(mon._monitor_pointer, do_lock)
			{}
		};

		monitor(const T &object): _monitor_pointer(new T(object))
		{}
		template<typename U, typename M>
		monitor(monitor<U, M> &other)
		{
			typename monitor<U, M>::scoped_lock lock(other);
			_monitor_pointer.reset(new T((*lock)));
		}

		monitor& operator=(const T &rhs)
		{
			scoped_lock lock(*this);
			*lock = rhs;
			return *this;
		}
		template<typename U, typename M>
		monitor& operator=(monitor<U, M> &rhs)
		{
			if(&rhs == this) return *this;

			boost::scoped_ptr<T> temp;
			/* Avoid locking the mutexes of both this monitor and the
			other monitor simultaneously, since we don't want to invite
			any potential locking order violations. */
			{
				typename monitor<U, M>::scoped_lock other_lock(rhs);
				temp.reset(new T(*other_lock));
			}
			return *this = *temp;
		}
	protected:
		friend class try_monitor<T, Mutex>;
		friend class timed_monitor<T, Mutex>;

		monitor_ptr<T, Mutex> _monitor_pointer;
	};

	template<typename T, typename Mutex = boost::try_mutex>
	class try_monitor: public monitor<T, Mutex>
	{
		typedef monitor<T, Mutex> base_class;
	public:
		class scoped_try_lock: public detail::monitor_scoped_try_lock<T, Mutex>
		{
			typedef typename detail::monitor_scoped_try_lock<T, Mutex> base_class;
		public:
			scoped_try_lock(monitor<T, Mutex> &mon): base_class(mon._monitor_pointer)
			{}
			scoped_try_lock(monitor<T, Mutex> &mon, bool do_lock):
				base_class(mon._monitor_pointer, do_lock)
			{}
		};

		try_monitor(const T &object): base_class(object)
		{}
		template<typename U, typename M>
		try_monitor(monitor<U, M> &other): base_class(other)
		{}
	};

	template<typename T, typename Mutex = boost::timed_mutex>
	class timed_monitor: public try_monitor<T, Mutex>
	{
		typedef try_monitor<T, Mutex> base_class;
	public:
		class scoped_timed_lock: public detail::monitor_scoped_timed_lock<T, Mutex>
		{
			typedef typename detail::monitor_scoped_timed_lock<T, Mutex> base_class;
		public:
			template<typename Timeout>
			scoped_timed_lock(monitor<T, Mutex> &mon, const Timeout &t):
				base_class(mon._monitor_pointer, t)
			{}
			scoped_timed_lock(monitor<T, Mutex> &mon, bool do_lock):
				base_class(mon._monitor_pointer, do_lock)
			{}
		};

		timed_monitor(const T &object): base_class(object)
		{}
		template<typename U, typename M>
		timed_monitor(monitor<U, M> &other): base_class(other)
		{}
	};
};

#endif // _POET_MONITOR_HPP
