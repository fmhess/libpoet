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
	namespace detail
	{
		template<typename MonitorPtr, typename T, typename Mutex>
		class try_monitor_template;
		template<typename MonitorPtr, typename T, typename Mutex>
		class timed_monitor_template;
		
		template<typename MonitorPtr, typename T, typename Mutex>
		class monitor_template
		{
		public:
			typedef T value_type;
			typedef Mutex mutex_type;

			class scoped_lock: public detail::monitor_scoped_lock<T, Mutex>
			{
				typedef typename detail::monitor_scoped_lock<T, Mutex> base_class;
			public:
				scoped_lock(monitor_template<MonitorPtr, T, Mutex> &mon): base_class(mon._monitor_pointer)
				{}
				scoped_lock(monitor_template<MonitorPtr, T, Mutex> &mon, bool do_lock):
					base_class(mon._monitor_pointer, do_lock)
				{}
			};
			
			monitor_template(const T &object): _monitor_pointer(new T(object))
			{}
			template<typename OtherMonitorPtr, typename U, typename M>
			monitor_template(monitor_template<OtherMonitorPtr, U, M> &other)
			{
				typename monitor_template<OtherMonitorPtr, U, M>::scoped_lock lock(other);
				_monitor_pointer.reset(new T((*lock)));
			}

			monitor_template& operator=(const T &rhs)
			{
				scoped_lock lock(*this);
				*lock = rhs;
				return *this;
			}
			template<typename OtherMonitorPtr, typename U, typename M>
			monitor_template& operator=(monitor_template<OtherMonitorPtr, U, M> &rhs)
			{
				if(&rhs == this) return *this;

				boost::scoped_ptr<T> temp;
				/* Avoid locking the mutexes of both this monitor and the
				other monitor simultaneously, since we don't want to invite
				any potential locking order violations. */
				{
					typename monitor_template<OtherMonitorPtr, U, M>::scoped_lock other_lock(rhs);
					temp.reset(new T(*other_lock));
				}
				return *this = *temp;
			}
		protected:
			friend class try_monitor_template<MonitorPtr, T, Mutex>;
			friend class timed_monitor_template<MonitorPtr, T, Mutex>;
			
			MonitorPtr _monitor_pointer;
		};
		
		template<typename MonitorPtr, typename T, typename Mutex>
		class try_monitor_template: public monitor_template<MonitorPtr, T, Mutex>
		{
			typedef monitor_template<MonitorPtr, T, Mutex> base_class;
		public:
			class scoped_try_lock: public detail::monitor_scoped_try_lock<T, Mutex>
			{
				typedef typename detail::monitor_scoped_try_lock<T, Mutex> base_class;
			public:
				scoped_try_lock(monitor_template<MonitorPtr, T, Mutex> &mon): base_class(mon._monitor_pointer)
				{}
				scoped_try_lock(monitor_template<MonitorPtr, T, Mutex> &mon, bool do_lock):
					base_class(mon._monitor_pointer, do_lock)
				{}
			};
			
			try_monitor_template(const T &object): base_class(object)
			{}
			template<typename OtherMonitorPtr, typename U, typename M>
			try_monitor_template(monitor_template<OtherMonitorPtr, U, M> &other): base_class(other)
			{}
		};
	
		template<typename MonitorPtr, typename T, typename Mutex>
		class timed_monitor_template: public try_monitor_template<MonitorPtr, T, Mutex>
		{
			typedef try_monitor_template<MonitorPtr, T, Mutex> base_class;
		public:
			class scoped_timed_lock: public detail::monitor_scoped_timed_lock<T, Mutex>
			{
				typedef typename detail::monitor_scoped_timed_lock<T, Mutex> base_class;
			public:
				template<typename Timeout>
				scoped_timed_lock(monitor_template<MonitorPtr, T, Mutex> &mon, const Timeout &t):
					base_class(mon._monitor_pointer, t)
				{}
				scoped_timed_lock(monitor_template<MonitorPtr, T, Mutex> &mon, bool do_lock):
					base_class(mon._monitor_pointer, do_lock)
				{}
			};
			
			timed_monitor_template(const T &object): base_class(object)
			{}
			template<typename OtherMonitorPtr, typename U, typename M>
			timed_monitor_template(monitor_template<OtherMonitorPtr, U, M> &other): base_class(other)
			{}
		};
	};

	template<typename T, typename Mutex = boost::mutex>
	class monitor: public detail::monitor_template<monitor_ptr<T, Mutex>, T, Mutex>
	{
	private:
		typedef typename detail::monitor_template<monitor_ptr<T, Mutex>, T, Mutex> base_class;
	public:
		monitor(const T &object): base_class(object)
		{}
		template<typename U, typename M>
		monitor(monitor<U, M> &other): base_class(other)
		{}
	};
	
	template<typename T, typename Mutex = boost::try_mutex>
	class try_monitor: public detail::try_monitor_template<try_monitor_ptr<T, Mutex>, T, Mutex>
	{
	private:
		typedef typename detail::try_monitor_template<try_monitor_ptr<T, Mutex>, T, Mutex> base_class;
	public:
		try_monitor(const T &object): base_class(object)
		{}
		template<typename U, typename M>
		try_monitor(try_monitor<U, M> &other): base_class(other)
		{}
	};
	
	template<typename T, typename Mutex = boost::timed_mutex>
	class timed_monitor: public detail::timed_monitor_template<timed_monitor_ptr<T, Mutex>, T, Mutex>
	{
	private:
		typedef typename detail::timed_monitor_template<timed_monitor_ptr<T, Mutex>, T, Mutex> base_class;
	public:
		timed_monitor(const T &object): base_class(object)
		{}
		template<typename U, typename M>
		timed_monitor(try_monitor<U, M> &other): base_class(other)
		{}
	};
};

#endif // _POET_MONITOR_HPP
