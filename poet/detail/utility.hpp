// copyright (c) Frank Mori Hess <fmhess@users.sourceforge.net>  2008-04-13

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_DETAIL_UTILITY_HPP
#define _POET_DETAIL_UTILITY_HPP

#include <boost/weak_ptr.hpp>

namespace poet
{
	namespace detail
	{
		// deadlock-free locking of a pair of locks with arbitrary locking order
		template<typename LockT, typename LockU>
		void lock_pair(LockT &a, LockU &b)
		{
			while(true)
			{
				a.lock();
				if(b.try_lock()) return;
				a.unlock();
				b.lock();
				if(a.try_lock()) return;
				b.unlock();
			}
		}

		template<typename T>
			boost::weak_ptr<T> make_weak(const boost::shared_ptr<T> &sp)
		{
			return boost::weak_ptr<T>(sp);
		}
	}
}

#endif	// _POET_DETAIL_UTILITY_HPP
