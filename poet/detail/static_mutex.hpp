/*
	A thread-safe singleton class which can be used build up a graph of a
	program's locking order for mutexes, and check it for locking
	order violations.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-10-26
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_STATIC_MUTEX_HPP
#define _POET_STATIC_MUTEX_HPP

#include <boost/thread/mutex.hpp>

namespace poet
{
	namespace detail
	{
		/* hack to add static mutex to classes without needing
		compiled lib to hold mutex definition */
		template<typename T> class static_mutex
		{
		public:
			static boost::mutex mutex;
		};
		template<typename T>
		boost::mutex static_mutex<T>::mutex;
	};
};

#endif // _POET_STATIC_MUTEX_HPP
