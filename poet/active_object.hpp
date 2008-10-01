/*
	This header defines some classes that can be used to build
	active objects.  See the paper "Active Object, An Object
	Behavioral Pattern for Concurrent Programming." by
	R. Greg Lavender and Douglas C. Schmidt
	for more information about active objects.

	Active objects that use Futures for both input parameters and
	return values can be chained together in pipelines or do
	dataflow-like processing, thereby achieving good concurrency.

	begin: Frank Hess <frank.hess@nist.gov>  2007-01-22
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_ACTIVE_OBJECT_H
#define _POET_ACTIVE_OBJECT_H

#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/signals2/signal.hpp>
#include <list>
#include <poet/detail/condition.hpp>
#include <poet/future.hpp>
#include <poet/future_select.hpp>

namespace poet
{
	class method_request_base
	{
	public:
		method_request_base() {}
		virtual ~method_request_base() {}
		virtual void run() = 0;
		virtual future<void> scheduling_guard() const = 0;
	};

	class activation_queue_base
	{
	public:
		typedef unsigned long size_type;

		virtual ~activation_queue_base() {}
		virtual void push_back(const boost::shared_ptr<method_request_base> &request) = 0;
		virtual boost::shared_ptr<method_request_base> get_request() = 0;
		virtual size_type size() const = 0;
		virtual bool empty() const = 0;
		virtual void wake() = 0;
	};

	class in_order_activation_queue: public activation_queue_base
	{
	public:
		virtual ~in_order_activation_queue() {}
		inline virtual void push_back(const boost::shared_ptr<method_request_base> &request);
		inline virtual boost::shared_ptr<method_request_base> get_request();
		virtual size_type size() const
		{
			boost::mutex::scoped_lock lock(_mutex);
			return _pendingRequests.size();
		}
		virtual bool empty() const
		{
			boost::mutex::scoped_lock lock(_mutex);
			return _pendingRequests.empty();
		}
		inline virtual void wake();
	private:
		typedef std::deque<boost::shared_ptr<method_request_base> > request_container_type;

		request_container_type _pendingRequests;
		mutable boost::mutex _mutex;
		promise<boost::shared_ptr<method_request_base> >_next_method_request;
		promise<boost::shared_ptr<method_request_base> >_wake_promise;
	};

	class out_of_order_activation_queue: public activation_queue_base
	{
		typedef future_selector<boost::shared_ptr<method_request_base> > selector_type;
	public:
		out_of_order_activation_queue()
		{}
		virtual ~out_of_order_activation_queue() {}

		inline virtual void push_back(const boost::shared_ptr<method_request_base> &request);
		inline virtual boost::shared_ptr<method_request_base> get_request();
		virtual size_type size() const
		{
			const selector_type::difference_type size = _selector.size();
			BOOST_ASSERT(size >= 0);
			return size;
		}
		virtual bool empty() const
		{
			BOOST_ASSERT(_selector.size() >= 0);
			return _selector.size() == 0;
		}
		inline virtual void wake();
	private:
		mutable boost::mutex _mutex;
		selector_type _selector;
		promise<boost::shared_ptr<method_request_base> >_wake_promise;
	};

	class scheduler_base
	{
	public:
		virtual ~scheduler_base()
		{}
		virtual void post_method_request(const boost::shared_ptr<method_request_base> &methodRequest) = 0;
		virtual void kill() = 0;
		virtual void join() = 0;
	};

	namespace detail
	{
		class scheduler_impl
		{
		public:
			inline scheduler_impl(const boost::shared_ptr<activation_queue_base> &activationQueue);
			~scheduler_impl() {}
			inline void post_method_request(const boost::shared_ptr<method_request_base> &methodRequest);
			inline void kill();
			inline void detach();
			inline bool mortallyWounded() const;
			static inline void dispatcherThreadFunction(const boost::shared_ptr<scheduler_impl> &shared_this);
		private:
			inline bool dispatch();
			bool detached() const
			{
				return _detached;
			}

			boost::shared_ptr<activation_queue_base> _activationQueue;
			poet::promise<boost::shared_ptr<method_request_base> > _wake_promise;
			mutable boost::mutex _mutex;
			bool _mortallyWounded;
			bool _detached;
		};
	}

	class scheduler: public scheduler_base
	{
	public:
		inline scheduler(const boost::shared_ptr<activation_queue_base> &activationQueue =
			boost::shared_ptr<activation_queue_base>(new out_of_order_activation_queue));
		virtual ~scheduler()
		{
			_pimpl->detach();
		}
		virtual void post_method_request(const boost::shared_ptr<method_request_base> &methodRequest)
		{
			_pimpl->post_method_request(methodRequest);
		}
		virtual void kill()
		{
			_pimpl->kill();
		}
		inline virtual void join();
	private:
		boost::shared_ptr<detail::scheduler_impl> _pimpl;
		boost::shared_ptr<boost::thread> _dispatcherThread;
	};
}

#include <poet/detail/active_object.cpp>

#endif // _POET_ACTIVE_OBJECT_H
