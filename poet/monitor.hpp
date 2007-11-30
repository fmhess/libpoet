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

#include <poet/monitor_ptr.hpp>
#include <boost/scoped_ptr.hpp>

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
			scoped_lock(monitor<T, Mutex> &mon): base_class(mon._monitor_pointer)
			{}
		};
		monitor()
		{}
		monitor(const T &object): _monitor_pointer(new T(object))
		{}
		template<typename U, typename M>
		monitor(const monitor<U, M> &other)
		{
			typename monitor<U, M>::scoped_lock lock(other);
			_monitor_pointer.reset(new T((*lock)));
		}
		virtual ~monitor() {}

		monitor& operator=(const T &rhs)
		{
			scoped_lock lock(*this);
			*lock = rhs;
			return *this;
		}
		template<typename U, typename M>
		monitor& operator=(const monitor<U, M> &rhs)
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
	private:

		monitor_ptr<T, Mutex> _monitor_pointer;
	};

};

#endif // _POET_MONITOR_HPP
