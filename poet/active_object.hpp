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

namespace poet
{
	/*! \brief Base class for method requests. */
	class method_request_base
	{
	protected:
		typedef boost::signal<void ()> update_signal_type;
	public:
		/*! Slot type used by connectUpdate().*/
		typedef boost::signal<void ()>::slot_type update_slot_type;

		/*! Constructor. */
		method_request_base(): _cancelled(false) {}
		/*! Virtual destructor. */
		virtual ~method_request_base() {}
		/*! run() is called by schedulers to execute the method request. */
		virtual void run() = 0;
		/*! Schedulers will not execute the method request until ready()
		returns true.  The default implementation always returns true. */
		virtual bool ready() const {return true;}
		/*! If a method request is still waiting in the scheduler's activation queue,
		cancel() will cause it to be dropped from the queue without being executed.  */
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
		/*! \returns true if the method request has been cancelled.  */
		virtual bool cancelled() const
		{
			boost::mutex::scoped_lock lock(_cancelMutex);
			return _cancelled;
		}
		/*! Connect a slot to the method request's "update" signal.  The slot
		will be run whenever the status of the method request
		changes, for example if it is cancelled, or if its "ready" state changes.  */
		boost::signalslib::connection connect_update(const update_slot_type &slot)
		{
			return update_signal.connect(slot);
		}
	protected:
		update_signal_type update_signal;
	private:
		mutable boost::mutex _cancelMutex;
		bool _cancelled;
	};

	/*! \brief Method request base class with support for cancellation via return value.
	*/
	template<typename ResultType>
	class method_request: public method_request_base, public boost::postconstructible,
    public boost::enable_shared_from_this<method_request<ResultType> >
	{
	public:
		/*! ResultType */
		typedef ResultType result_type;

		/*! Virtual destructor */
		virtual ~method_request()
		{}
		/*! Calls method_request_base::cancel() and additionally cancels
		the method_request's return value. */
		virtual void cancel()
		{
			method_request_base::cancel();
			future<result_type>(return_value).cancel();
		}
	protected:
		typedef typename future<ResultType>::update_slot_type future_slot_type;

		/*! Protected constructor.  This class uses a post-constructor provided by
		the deconstruct_ptr() framework, so derived classes
		should
		only be instantiated via calls to boost::deconstruct_ptr().  It is recommended that derived
		classes use protected/private constructors and provide a static factory
		method which calls boost::deconstruct_ptr().
		\param returnValue The future which will be cancelled if this method_request
		is cancelled.  Conversely, cancelling <em>returnValue</em> will also cause
		this method_request to be cancelled.
		*/
		method_request(const promise<result_type> &returnValue):
			return_value(returnValue)
		{}
		/*! Post-constructor.  Performs some signal-slot connection with automatic
		lifetime tracking which cannot be performed in the constructor due
		to the need for a shared_ptr from <em>this</em>. */
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

	/*! \brief Base class for activation queues. */
	class activation_queue_base
	{
	public:
		typedef unsigned long size_type;

		/*! Virtual destructor. */
		virtual ~activation_queue_base() {}
		/*! Adds a new method request to the activation queue. */
		virtual void push_back(const boost::shared_ptr<method_request_base> &request) = 0;
		/*! \returns The next method request which is ready to run.  Returns a null shared_ptr
		if there are no method requests ready.
		*/
		virtual boost::shared_ptr<method_request_base> get_request() = 0;
		/*! Empties all method requests from the queue. */
		virtual void clear() = 0;
		/*! \returns the number of method requests waiting in the queue. */
		virtual size_type size() const = 0;
		/*! \returns true if size() is zero. */
		virtual bool empty() const = 0;
	};

	/*! \brief An activation queue which always keeps method requests in FIFO order.

		An in_order_activation_queue will never skip over method requests that aren't
		ready yet.  If you don't require the method requests to be executed in
		the exact order they were received, use an out_of_order_activation_queue
		instead.
	*/
	class in_order_activation_queue: public activation_queue_base
	{
	public:
		/*! Virtual destructor. */
		virtual ~in_order_activation_queue() {}
		inline virtual void push_back(const boost::shared_ptr<method_request_base> &request);
		/*! \returns The oldest method request in the queue.  If the oldest method
		request is not ready to execute, then a null shared_ptr is returned.
		*/
		inline virtual boost::shared_ptr<method_request_base> get_request();
		virtual void clear()
		{
			boost::mutex::scoped_lock lock(_mutex);
			_pendingRequests.clear();
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
	protected:
		inline boost::shared_ptr<method_request_base> unlockedGetRequest();

		typedef std::list<boost::shared_ptr<method_request_base> > list_type;
		list_type _pendingRequests;
		mutable boost::mutex _mutex;
	};

	/*! \brief An activation queue which can reorder method requests.

	An out_of_order_activation_queue will return the oldest method request
	which is currently ready for execution.  Thus, a single method request
	which is not ready will never prevent other method requests
	which are ready from running.
	*/
	class out_of_order_activation_queue: public in_order_activation_queue
	{
	public:
		virtual ~out_of_order_activation_queue() {}
		/*! \returns The oldest method request in the queue
		which is currently ready for execution. */
		inline virtual boost::shared_ptr<method_request_base> get_request();
	};

	/*! \brief Base class for schedulers.

	A scheduler creates its own thread
	and executes method requests which are passed to it through its
	activation queue.  */
	class scheduler_base
	{
	public:
		/*! Virtual destructor. */
		virtual ~scheduler_base()
		{}
		/*! Adds <em>methodRequest</em> to the scheduler's activation queue. */
		virtual void post_method_request(const boost::shared_ptr<method_request_base> &methodRequest) = 0;
		/*! Manually force the scheduler to wake up and check for runnable method requests.
		This is usually not required, as schedulers should automatically check
		there activation queue when post_method_request is called, or when any method request
		in the activation queue emits its "update" signal. */
		virtual void wake() = 0;
		/*! Tells scheduler thread to exit.  The scheduler thread may still be running after
		this function returns. */
		virtual void kill() = 0;
		/*! Blocks until the scheduler thread exits. */
		virtual void join() = 0;
	};

	namespace detail
	{
		class scheduler_impl
		{
		public:
			scheduler_impl(int millisecTimeout, const boost::shared_ptr<activation_queue_base> &activationQueue);
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
			inline void setWakePending(bool value);
			bool detached() const
			{
				return _detached;
			}

			boost::shared_ptr<activation_queue_base> _activationQueue;
			poet::detail::condition _wakeCondition;
			bool _wakePending;
			mutable boost::mutex _mutex;
			bool _mortallyWounded;
			int _millisecTimeout;
			bool _detached;
		};
	}

	/*! \brief Execute method requests in a separate thread.
	*/
	class scheduler: public scheduler_base
	{
	public:
		/*!
		The scheduler constructer will create a new thread of execution, where the scheduler
		will execute method requests.

		\param millisecTimeout Specifies a polling interval for the scheduler to check for any
		ready method requests in its activation queue.  If millisecTimeout is less than zero,
		no polling is performed.  Polling is usually not required, as schedulers will always check
		there activation queue when post_method_request is called, or when any method request
		in the activation queue emits its "update" signal.
		\param activationQueue Allows use of a customized activation queue.  By default, an
		out_of_order_activation_queue is used.
		*/
		scheduler(int millisecTimeout = -1, const boost::shared_ptr<activation_queue_base> &activationQueue =
			boost::shared_ptr<activation_queue_base>(new out_of_order_activation_queue));
		/*! The scheduler thread will continue to run after the scheduler object is destroyed, until
		all method requests in its activation queue have been dispatched.
		*/
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
