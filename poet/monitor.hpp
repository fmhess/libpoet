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

#include <algorithm>
#include <boost/optional.hpp>
#include <boost/scoped_ptr.hpp>
#include <poet/detail/monitor_locks.hpp>
#include <poet/monitor_ptr.hpp>
#include <poet/mutex_properties.hpp>

namespace poet
{
	namespace detail
	{
		template<typename T, typename Mutex, enum mutex_model>
		class specialized_monitor
		{
		private:
			specialized_monitor() {}
		};

		template<typename T, typename Mutex>
		class specialized_monitor<T, Mutex, mutex_concept>
		{
		public:
			typedef T value_type;
			typedef Mutex mutex_type;

			class scoped_lock: public monitor_ptr<T, Mutex>::scoped_lock
			{
				typedef typename monitor_ptr<T, Mutex>::scoped_lock base_class;
			public:
				scoped_lock(specialized_monitor<T, Mutex, mutex_concept> &mon):
					base_class(mon._monitor_pointer)
				{}
				scoped_lock(specialized_monitor<T, Mutex, mutex_concept> &mon, bool do_lock):
					base_class(mon._monitor_pointer, do_lock)
				{}
			};

			specialized_monitor(): _monitor_pointer(new T)
			{}
			specialized_monitor(const T &object): _monitor_pointer(new T(object))
			{}
			template<typename U, typename M>
			specialized_monitor(specialized_monitor<U, M, mutex_concept> &other)
			{
				typename specialized_monitor<U, M, mutex_concept>::scoped_lock lock(other);
				_monitor_pointer.reset(new T((*lock)));
			}
			virtual ~specialized_monitor() {}

			specialized_monitor& operator=(const T &rhs)
			{
				scoped_lock lock(*this);
				*lock = rhs;
				return *this;
			}
			template<typename U, typename M>
			specialized_monitor& operator=(specialized_monitor<U, M, mutex_concept> &rhs)
			{
				if(&rhs == this) return *this;

				boost::optional<T> temp;
				/* Avoid locking the mutexes of both this monitor and the
				other monitor simultaneously, since we don't want to invite
				any potential locking order violations. */
				{
					typename specialized_monitor<U, M, mutex_concept>::scoped_lock other_lock(rhs);
					temp = *other_lock;
				}
				scoped_lock lock(*this);
				swap(*lock, *temp);
				return *this;
			}

			template<typename M>
			void swap(specialized_monitor<T, M, mutex_concept> &other)
			{
				boost::optional<T> temp;
				/* Avoid locking the mutexes of both this monitor and the
				other monitor simultaneously, since we don't want to invite
				any potential locking order violations. */
				{
					typename specialized_monitor<T, M, mutex_concept>::scoped_lock other_lock(other);
					temp = *other_lock;
				}
				{
					scoped_lock lock(*this);
					swap(*lock, *temp);
				}
				{
					typename specialized_monitor<T, M, mutex_concept>::scoped_lock other_lock(other);
					swap(*other_lock, *temp);
				}
			}

		protected:
			template<typename U, typename M, enum mutex_model model>
			friend class specialized_monitor;

			monitor_ptr<T, Mutex> _monitor_pointer;
		};

		template<typename T, typename Mutex>
		class specialized_monitor<T, Mutex, try_mutex_concept>:
			public specialized_monitor<T, Mutex, mutex_concept>
		{
			typedef specialized_monitor<T, Mutex, mutex_concept> base_class;
		public:
			class scoped_try_lock: public monitor_ptr<T, Mutex>::scoped_try_lock
			{
				typedef typename monitor_ptr<T, Mutex>::scoped_try_lock base_class;
			public:
				scoped_try_lock(specialized_monitor<T, Mutex, try_mutex_concept> &mon): base_class(mon._monitor_pointer)
				{}
				scoped_try_lock(specialized_monitor<T, Mutex, try_mutex_concept> &mon, bool do_lock):
					base_class(mon._monitor_pointer, do_lock)
				{}
			};

			specialized_monitor()
			{}
			specialized_monitor(const T &object): base_class(object)
			{}
			template<typename U, typename M, enum mutex_model model>
			specialized_monitor(specialized_monitor<U, M, model> &other): base_class(other)
			{}
		};

		template<typename T, typename Mutex>
		class specialized_monitor<T, Mutex, timed_mutex_concept>:
			public specialized_monitor<T, Mutex, try_mutex_concept>
		{
			typedef specialized_monitor<T, Mutex, try_mutex_concept> base_class;
		public:
			class scoped_timed_lock: public monitor_ptr<T, Mutex>::scoped_timed_lock
			{
				typedef typename monitor_ptr<T, Mutex>::scoped_timed_lock base_class;
			public:
				template<typename Timeout>
				scoped_timed_lock(specialized_monitor<T, Mutex, timed_mutex_concept> &mon, const Timeout &t):
					base_class(mon._monitor_pointer, t)
				{}
				scoped_timed_lock(specialized_monitor<T, Mutex, timed_mutex_concept> &mon, bool do_lock):
					base_class(mon._monitor_pointer, do_lock)
				{}
			};

			specialized_monitor()
			{}
			specialized_monitor(const T &object): base_class(object)
			{}
			template<typename U, typename M, enum mutex_model model>
			specialized_monitor(specialized_monitor<U, M, model> &other): base_class(other)
			{}
		};
	};

	template<typename T, typename Mutex = boost::mutex>
	class monitor: public detail::specialized_monitor<T, Mutex, mutex_properties<Mutex>::model>
	{
		typedef typename detail::specialized_monitor<T, Mutex, mutex_properties<Mutex>::model> base_class;
	public:
		monitor()
		{}
		monitor(const T &object): base_class(object)
		{}
		template<typename U, typename M>
		monitor(monitor<U, M> &other): base_class(other)
		{}
	};

	template<typename T, typename Mutex>
	void swap(poet::monitor<T, Mutex> &mon0, poet::monitor<T, Mutex> &mon1)
	{
		mon0.swap(mon1);
	}
};

#endif // _POET_MONITOR_HPP
