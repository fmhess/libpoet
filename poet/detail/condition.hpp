/*
	An slightly easier to use wrapper around boost::condition.

	begin: Frank Hess <frank.hess@nist.gov>  2007-01-30
*/
/* This software was developed at the National Institute of Standards and
 * Technology by employees of the Federal Government in the course of
 * their official duties. Pursuant to title 17 Section 105 of the United
 * States Code this software is not subject to copyright protection and is
 * in the public domain. This is an experimental system. NIST assumes no
 * responsibility whatsoever for its use by other parties, and makes no
 * guarantees, expressed or implied, about its quality, reliability, or
 * any other characteristic. We would appreciate acknowledgement if the
 * software is used.
 */

#ifndef _POET_CONDITION_HPP
#define _POET_CONDITION_HPP

#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>

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
				boost::mutex::scoped_lock lock(mutex);
				wait(lock, predicate);
			}
			template <typename Pred> void locking_timed_wait(const boost::xtime &xt, Pred predicate)
			{
				boost::mutex::scoped_lock lock(mutex);
				timed_wait(lock, xt, predicate);
			}
			mutable boost::mutex mutex;
		};
	}
}

#endif // _POET_CONDITION_HPP
