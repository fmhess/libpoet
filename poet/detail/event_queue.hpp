/*
	Simple event queue inspired by asio::io_service, but without needing
	thread-unsafe reset.

	begin: Frank Hess <frank.hess@nist.gov>  2008-05-31
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_EVENT_QUEUE_HPP
#define _POET_EVENT_QUEUE_HPP

#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <deque>
namespace poet
{
	namespace detail
	{
		class event_queue
		{
		public:
			typedef boost::function<void ()> event_type;
			std::size_t poll_one()
			{
				std::size_t count = 0;
				event_type event;
				{
					boost::unique_lock<boost::mutex> lock(_mutex);
					if(_queue.empty())
					{
						return count;
					}else
					{
						event = _queue.front();
						_queue.pop_front();
						++count;
					}
				}
				event();
				return count;
			}
			std::size_t poll()
			{
				std::size_t count = 0;
				while(poll_one() == 1) ++count;
				return count;
			}
			template<typename Event>
			void post(const Event &event)
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				_queue.push_back(event);
			}
		private:
			std::deque<event_type> _queue;
			boost::mutex _mutex;
		};
	}
}

#endif // _POET_EVENT_QUEUE_HPP
