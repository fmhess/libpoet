/*
	A wrapper which automatically locks/unlocks a mutex whenever the wrapped
	objects members are accessed.  See "Wrapping C++ Member Function Calls"
	by Bjarne Stroustroup at http://www.research.att.com/~bs/wrapper.pdf

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-08-27
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_BASE_HPP
#define _POET_MONITOR_BASE_HPP

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition.hpp>

namespace boost
{
	class mutex;
};

namespace poet
{
	namespace detail
	{
		class monitor_synchronizer_base
		{
		public:
			monitor_synchronizer_base(): _condition(new boost::condition())
			{}
			virtual ~monitor_ptr_base()
			{}
		protected:
			boost::shared_ptr<boost::condition> _condition;
		private:
			friend class monitor_base;

			virtual void wait() const = 0;
			virtual void wait(const boost::function<bool ()> &pred) const = 0;
			void notify_one() const
			{
				_condition->notify_one();
			}
			void notify_all() const
			{
				_condition->notify_all();
			}
		};

		template<typename Mutex>
		class monitor_synchronizer: public monitor_synchronizer_base
		{
		public:
			Mutex _mutex;
			boost::weak_ptr<typename Mutex::scoped_lock> _current_lock;
		};
	};
};

#endif // _POET_MONITOR_BASE_HPP
