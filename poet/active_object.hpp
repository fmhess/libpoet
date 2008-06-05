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

#include <boost/deconstruct_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread_safe_signal.hpp>
#include <list>
#include <poet/detail/condition.hpp>
#include <poet/future.hpp>
#include <poet/future_select.hpp>

namespace poet
{
	class method_request_base
	{
	protected:
		typedef boost::signal<void ()> update_signal_type;
	public:
		typedef boost::signal<void ()>::slot_type update_slot_type;

		method_request_base(): _cancelled(false) {}
		virtual ~method_request_base() {}
		virtual void run() = 0;
		virtual bool ready() const {return true;}
		virtual void cancel()
		{
			bool emitSignal = false;
			{
				boost::mutex::scoped_lock lock(_cancelMutex);
				if(_cancelled == false)
				{
					emitSignal = true;
					_cancelled = true;
				}
			}
			if(emitSignal) update_signal();
		}
		virtual bool cancelled() const
		{
			boost::mutex::scoped_lock lock(_cancelMutex);
			return _cancelled;
		}
		boost::signalslib::connection connect_update(const update_slot_type &slot)
		{
			return update_signal.connect(slot);
		}
		virtual future<void> dependencies() const = 0;
	protected:
		update_signal_type update_signal;
	private:
		mutable boost::mutex _cancelMutex;
		bool _cancelled;
	};

	template<typename ResultType>
	class method_request: public method_request_base, public boost::postconstructible,
    public boost::enable_shared_from_this<method_request<ResultType> >
	{
	public:
		typedef ResultType result_type;

		virtual ~method_request()
		{}
		virtual void cancel()
		{
			method_request_base::cancel();
			future<result_type>(return_value).cancel();
		}
	protected:
		typedef typename future<ResultType>::update_slot_type future_slot_type;

		method_request(const promise<result_type> &returnValue):
			return_value(returnValue)
		{}
		virtual void postconstruct()
		{
			future<result_type>(return_value).connect_update(
				future_slot_type(&method_request<ResultType>::handle_return_value_update, this).track(this->shared_from_this()));
		}
		promise<result_type> return_value;
	private:
		void handle_return_value_update()
		{
			if(future<result_type>(return_value).has_exception())
			{
				method_request_base::cancel();
			}
		}
	};

	class activation_queue_base
	{
	public:
		typedef unsigned long size_type;

		virtual ~activation_queue_base() {}
		virtual void push_back(const boost::shared_ptr<method_request_base> &request) = 0;
		virtual boost::shared_ptr<method_request_base> get_request() = 0;
		virtual void clear() = 0;
		virtual size_type size() const = 0;
		virtual bool empty() const = 0;
		virtual future<void> queue_is_ready() const = 0;
		virtual void pop_queue_is_ready() = 0;
		virtual void wake() = 0;
	};

	class in_order_activation_queue: public activation_queue_base
	{
	public:
		virtual ~in_order_activation_queue() {}
		inline virtual void push_back(const boost::shared_ptr<method_request_base> &request);
		inline virtual boost::shared_ptr<method_request_base> get_request();
		virtual void clear()
		{
			boost::mutex::scoped_lock lock(_mutex);
			_pendingRequests.clear();
			//FIXME: need to clear or reset _selector
		}
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
		virtual future<void> queue_is_ready() const
		{
			return _selector.selected();
		}
		virtual void pop_queue_is_ready()
		{
			_selector.pop_selected();
		}
		virtual void wake()
		{
			future<int> ready_future = 1;
			_selector.push(ready_future);
		}
	protected:
		inline boost::shared_ptr<method_request_base> unlockedGetRequest();

		typedef std::list<boost::shared_ptr<method_request_base> > list_type;
		list_type _pendingRequests;
		mutable boost::mutex _mutex;
		poet::future_selector<void> _selector;
	};

	class out_of_order_activation_queue: public in_order_activation_queue
	{
	public:
		out_of_order_activation_queue(): _next(in_order_activation_queue::_pendingRequests.end())
		{}
		virtual ~out_of_order_activation_queue() {}
		inline virtual void push_back(const boost::shared_ptr<method_request_base> &request);
		inline virtual boost::shared_ptr<method_request_base> get_request();
	private:
		list_type::iterator _next;
	};

	class scheduler_base
	{
	public:
		virtual ~scheduler_base()
		{}
		virtual void post_method_request(const boost::shared_ptr<method_request_base> &methodRequest) = 0;
		virtual void wake() = 0;
		virtual void kill() = 0;
		virtual void join() = 0;
	};

	namespace detail
	{
		class scheduler_impl
		{
		public:
			inline scheduler_impl(int millisecTimeout, const boost::shared_ptr<activation_queue_base> &activationQueue);
			~scheduler_impl() {}
			inline void post_method_request(const boost::shared_ptr<method_request_base> &methodRequest);
			inline void wake();
			inline void kill();
			inline void detach();
			inline bool mortallyWounded() const;
			static inline void dispatcherThreadFunction(const boost::shared_ptr<scheduler_impl> &shared_this);
		private:
			inline bool dispatch();
			inline bool wakePending() const;
			bool detached() const
			{
				return _detached;
			}

			boost::shared_ptr<activation_queue_base> _activationQueue;
			poet::detail::condition _wakeCondition;
			mutable boost::mutex _mutex;
			bool _mortallyWounded;
			int _millisecTimeout;
			bool _detached;
		};
	}

	class scheduler: public scheduler_base
	{
	public:
		inline scheduler(int millisecTimeout = -1, const boost::shared_ptr<activation_queue_base> &activationQueue =
			boost::shared_ptr<activation_queue_base>(new out_of_order_activation_queue));
		virtual ~scheduler()
		{
			_pimpl->detach();
		}
		virtual void post_method_request(const boost::shared_ptr<method_request_base> &methodRequest)
		{
			_pimpl->post_method_request(methodRequest);
		}
		virtual void wake()
		{
			_pimpl->wake();
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
