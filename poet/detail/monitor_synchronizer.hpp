/*
	A wrapper which automatically locks/unlocks a mutex whenever the wrapped
	objects members are accessed.  See "Wrapping C++ Member Function Calls"
	by Bjarne Stroustroup at http://www.research.att.com/~bs/wrapper.pdf

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-08-27
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_SYNCHRONIZER_HPP
#define _POET_MONITOR_SYNCHRONIZER_HPP

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
			typedef boost::function<void (boost::condition &, const boost::function<bool ()> &)> wait_function_type;

			virtual ~monitor_synchronizer_base()
			{}

			void wait()
			{
				wait_function_type wait_func = _wait_func;
				_wait_func(_condition, boost::function<bool ()>());
				// restore _wait_func after we have lock again
				_wait_func = wait_func;
			}
			void wait(const boost::function<bool ()> &pred)
			{
				wait_function_type wait_func = _wait_func;
				_wait_func(_condition, pred);
				// restore _wait_func after we have lock again
				_wait_func = wait_func;
			}
			void notify_one()
			{
				_condition.notify_one();
			}
			void notify_all()
			{
				_condition.notify_all();
			}
			void set_wait_function(const wait_function_type &wait_func)
			{
				_wait_func = wait_func;
			}
		protected:
			boost::condition _condition;
			wait_function_type _wait_func;
		};

		template<typename Mutex>
		class monitor_synchronizer: public monitor_synchronizer_base
		{
		public:
			Mutex _mutex;
		};
	};
};

#endif // _POET_MONITOR_SYNCHRONIZER_HPP
