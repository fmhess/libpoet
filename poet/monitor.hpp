/*
	An experimental alternative to monitor_ptr that stores the wrapped
	object by value instead of acting like a pointer.

	begin: Frank Mori Hess <fmhess@users.sourceforge.net>  2007-11-29
	copyright (c) Frank Mori Hess 2007-2008
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_HPP
#define _POET_MONITOR_HPP

#include <algorithm>
#include <boost/optional.hpp>
#include <boost/preprocessor/repetition.hpp>
#include <boost/scoped_ptr.hpp>
#include <poet/detail/monitor_locks.hpp>
#include <poet/detail/preprocessor_macros.hpp>
#include <poet/monitor_ptr.hpp>
#include <poet/mutex_properties.hpp>

#ifndef POET_MONITOR_MAX_CONSTRUCTOR_ARGS
#define POET_MONITOR_MAX_CONSTRUCTOR_ARGS 10
#endif

namespace poet
{
	template<typename T, typename Mutex = boost::mutex>
	class monitor
	{
	public:
		typedef T value_type;
		typedef Mutex mutex_type;

		class scoped_lock: public monitor_ptr<T, Mutex>::scoped_lock
		{
			typedef typename monitor_ptr<T, Mutex>::scoped_lock base_class;
		public:
			scoped_lock(monitor<T, Mutex> &mon):
				base_class(mon._monitor_pointer)
			{}
			explicit scoped_lock(const monitor<T, Mutex> &mon):
				base_class(mon._monitor_pointer)
			{}
			scoped_lock(monitor<T, Mutex> &mon, bool do_lock):
				base_class(mon._monitor_pointer, do_lock)
			{}
			explicit scoped_lock(const monitor<T, Mutex> &mon, bool do_lock):
				base_class(mon._monitor_pointer, do_lock)
			{}
		};

		class scoped_try_lock: public monitor_ptr<T, Mutex>::scoped_try_lock
		{
			typedef typename monitor_ptr<T, Mutex>::scoped_try_lock base_class;
		public:
			scoped_try_lock(monitor<T, Mutex> &mon): base_class(mon._monitor_pointer)
			{}
			explicit scoped_try_lock(const monitor<T, Mutex> &mon): base_class(mon._monitor_pointer)
			{}
			scoped_try_lock(monitor<T, Mutex> &mon, bool do_lock):
				base_class(mon._monitor_pointer, do_lock)
			{}
			explicit scoped_try_lock(const monitor<T, Mutex> &mon, bool do_lock):
				base_class(mon._monitor_pointer, do_lock)
			{}
		};

		class scoped_timed_lock: public monitor_ptr<T, Mutex>::scoped_timed_lock
		{
			typedef typename monitor_ptr<T, Mutex>::scoped_timed_lock base_class;
		public:
			template<typename Timeout>
			scoped_timed_lock(monitor<T, Mutex> &mon, const Timeout &t):
				base_class(mon._monitor_pointer, t)
			{}
			template<typename Timeout>
			explicit scoped_timed_lock(const monitor<T, Mutex> &mon, const Timeout &t):
				base_class(mon._monitor_pointer, t)
			{}
			scoped_timed_lock(monitor<T, Mutex> &mon, bool do_lock):
				base_class(mon._monitor_pointer, do_lock)
			{}
			explicit scoped_timed_lock(const monitor<T, Mutex> &mon, bool do_lock):
				base_class(mon._monitor_pointer, do_lock)
			{}
		};

		monitor(): _monitor_pointer(new T)
		{}
		monitor(const T &object): _monitor_pointer(new T(object))
		{}
		monitor(const monitor &other)
		{
			scoped_lock lock(other);
			_monitor_pointer.reset(new T((*lock)));
		}
		template<typename U, typename M>
		monitor(const monitor<U, M> &other)
		{
			typename monitor<U, M>::scoped_lock lock(other);
			_monitor_pointer.reset(new T((*lock)));
		}
#define POET_BASE_MONITOR_TEMPLATE_CONSTRUCTOR(z, n, dummy) \
	template<POET_REPEATED_TYPENAMES(n, U)> \
	monitor(POET_REPEATED_ARG_DECLARATIONS(n, U)): _monitor_pointer(new T(POET_REPEATED_ARG_NAMES(n, arg))) \
	{}
		BOOST_PP_REPEAT_FROM_TO(1, BOOST_PP_INC(POET_MONITOR_MAX_CONSTRUCTOR_ARGS), POET_BASE_MONITOR_TEMPLATE_CONSTRUCTOR, x)
#undef POET_BASE_MONITOR_TEMPLATE_CONSTRUCTOR
		virtual ~monitor() {}

		monitor& operator=(const monitor &rhs)
		{
			return operator=<T, Mutex>(rhs);
		}
		template<typename U, typename M>
		monitor& operator=(const monitor<U, M> &rhs)
		{
			if(&rhs == this) return *this;

			boost::optional<T> temp;
			/* Avoid locking the mutexes of both this monitor and the
			other monitor simultaneously, since we don't want to invite
			any potential locking order violations. */
			{
				typename monitor<U, M>::scoped_lock other_lock(rhs);
				temp = *other_lock;
			}
			scoped_lock lock(*this);
			using std::swap;
			swap(*lock, *temp);
			return *this;
		}

		template<typename M>
		void _internal_swap(monitor<T, M> &other)
		{
			using std::swap;
			boost::optional<T> temp;
			/* Avoid locking the mutexes of both this monitor and the
			other monitor simultaneously, since we don't want to invite
			any potential locking order violations. */
			{
				typename monitor<T, M>::scoped_lock other_lock(other);
				temp = *other_lock;
			}
			{
				scoped_lock lock(*this);
				swap(*lock, *temp);
			}
			{
				typename monitor<T, M>::scoped_lock other_lock(other);
				swap(*other_lock, *temp);
			}
		}

		const monitor_ptr<T, Mutex> & operator->()
		{
			return _monitor_pointer;
		}
		monitor_ptr<const T, Mutex> operator->() const
		{
			return _monitor_pointer;
		}

		const monitor_ptr<T, Mutex> & get_monitor_ptr()
		{
			return _monitor_pointer;
		}
		monitor_ptr<const T, Mutex> get_monitor_ptr() const
		{
			return _monitor_pointer;
		}
	protected:
		template<typename U, typename M>
		friend class monitor;

		monitor_ptr<T, Mutex> _monitor_pointer;
	};

	template<typename T, typename Mutex>
	void swap(poet::monitor<T, Mutex> &mon0, poet::monitor<T, Mutex> &mon1)
	{
		mon0._internal_swap(mon1);
	}
};

#undef POET_SPECIALIZED_MONITOR_TEMPLATE_CONSTRUCTOR

#endif // _POET_MONITOR_HPP
