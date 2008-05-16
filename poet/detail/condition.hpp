/*
	An slightly easier to use wrapper around boost::condition.

	begin: Frank Hess <frank.hess@nist.gov>  2007-01-30
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_CONDITION_HPP
#define _POET_CONDITION_HPP

#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>

namespace poet
{
	namespace detail
	{
		class condition: public boost::condition
		{
		public:
			void locking_notify_all()
			{
				boost::mutex::scoped_lock lock(mutex);
				notify_all();
			}
			template <typename Pred> void locking_wait(Pred predicate)
			{
				boost::unique_lock<boost::mutex> lock(mutex);
				wait(lock, predicate);
			}
			template <typename Timeout, typename Pred> void locking_timed_wait(const Timeout &t, Pred predicate)
			{
				boost::unique_lock<boost::mutex> lock(mutex);
				timed_wait(lock, t, predicate);
			}
			mutable boost::mutex mutex;
		};
	}
}

#endif // _POET_CONDITION_HPP
