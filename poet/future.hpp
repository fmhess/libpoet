/*
	poet::future defines a templated future class which can be used,
	for example, to implement "active objects" and asynchronous function
	calls.  See the paper "Active Object, An Object Behavioral Pattern for
	Concurrent Programming." by R. Greg Lavender and Douglas C. Schmidt
	for more information about active objects and futures.

	Active objects that use futures for both input parameters and
	return values can be chained together in pipelines or do
	dataflow-like processing, thereby achieving good concurrency.

	begin: Frank Hess <frank.hess@nist.gov>  2007-01-22
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_FUTURE_H
#define _POET_FUTURE_H

#include <boost/assert.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/optional.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread_time.hpp>
#include <boost/thread_safe_signal.hpp>
#include <boost/weak_ptr.hpp>
#include <poet/detail/condition.hpp>
#include <poet/detail/event_queue.hpp>
#include <poet/detail/nonvoid.hpp>
#include <poet/exception_ptr.hpp>
#include <poet/exceptions.hpp>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

namespace poet
{
	template <typename T>
	class future;

	namespace detail
	{
		class future_body_untyped_base;

		/* class for holding wait callbacks.  Any thread can post a functor to the waiter_event_queue,
		but only future-waiting threads should pop them off and execute them. */
		class waiter_event_queue
		{
			typedef boost::signal<void (const event_queue::event_type &)> event_posted_type;
		public:
			typedef event_posted_type::slot_type slot_type;
			waiter_event_queue(boost::mutex &mutex, boost::condition &condition):
				_mutex(mutex), _condition(condition)
			{}

			void set_owner(const boost::shared_ptr<const void> &queue_owner)
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				boost::shared_ptr<waiter_event_queue> shared_this(queue_owner, this);
				_weak_this = shared_this;
			}
			template<typename Event>
			void post(const Event &event)
			{
				BOOST_ASSERT(_weak_this.expired() == false);
				_events.post(event);
				_event_posted(create_poll_event());
				boost::unique_lock<boost::mutex> lock(_mutex);
				_condition.notify_all();
			}
			void poll()
			{
				_events.poll();
			}
			event_queue::event_type create_poll_event()
			{
				event_queue::event_type event = boost::bind(&waiter_event_queue::poll_event_impl, _weak_this);
				return event;
			}
			boost::signalslib::connection connect_slot(const slot_type &slot)
			{
				return _event_posted.connect(slot);
			}
		private:
			static void poll_event_impl(const boost::weak_ptr<waiter_event_queue> &weak_this)
			{
				boost::shared_ptr<waiter_event_queue> shared_this = weak_this.lock();
				if(!shared_this) return;
				shared_this->poll();
			}

			event_queue _events;
			boost::mutex &_mutex;
			boost::condition &_condition;
			event_posted_type _event_posted;
			boost::weak_ptr<waiter_event_queue> _weak_this;
		};

		class future_body_untyped_base: public boost::enable_shared_from_this<future_body_untyped_base>
		{
		public:
			typedef boost::signal<void ()> update_signal_type;
			typedef update_signal_type::slot_type update_slot_type;

			future_body_untyped_base()
			{
			}
			virtual ~future_body_untyped_base()
			{
			}
			virtual bool ready() const = 0;
			virtual void join() const = 0;
			virtual bool timed_join(const boost::system_time &absolute_time) const = 0;
			virtual void cancel(const poet::exception_ptr &) = 0;
			virtual exception_ptr get_exception_ptr() const = 0;
			virtual waiter_event_queue& waiter_callbacks() const = 0;
			boost::signalslib::connection connectUpdate(const update_signal_type::slot_type &slot)
			{
				return _updateSignal.connect(slot);
			}
		protected:
			update_signal_type _updateSignal;
			mutable boost::mutex _mutex;
			mutable boost::condition _condition;
			mutable poet::exception_ptr _exception;
		};

		template <typename T> class future_body_base: public virtual future_body_untyped_base
		{
		public:
			virtual const typename nonvoid<T>::type& getValue() const = 0;
			virtual void setValue(const typename nonvoid<T>::type &value) = 0;
		};

		template <typename T> class future_body: public future_body_base<T>
		{
		public:
			future_body(): _waiter_callbacks(this->_mutex, this->_condition)
			{}
			future_body(const T &value): _value(value),
				_waiter_callbacks(this->_mutex, this->_condition)
			{}
			future_body(const poet::exception_ptr &ep, int):
				_waiter_callbacks(this->_mutex, this->_condition)
			{
				this->_exception = ep;
			}
			virtual ~future_body() {}
			virtual void setValue(const T &value)
			{
				bool emit_signal = false;
				{
					boost::unique_lock<boost::mutex> lock(this->_mutex);
					if(this->_exception == false && !_value)
					{
						_value = value;
						this->_condition.notify_all();
						emit_signal = true;
					}
				}
				if(emit_signal)
				{
					this->_updateSignal();
				}
			}
			virtual bool ready() const
			{
					boost::unique_lock<boost::mutex> lock(this->_mutex);
				return _value;
			}
			virtual const T& getValue() const
			{
				boost::unique_lock<boost::mutex> lock(this->_mutex);
				this->_condition.wait(lock, boost::bind(&future_body<T>::check_if_complete, this, &lock));
				if(this->_exception)
				{
					rethrow_exception(this->_exception);
				}
				BOOST_ASSERT(_value);
				return _value.get();
			}
			virtual void join() const
			{
				boost::unique_lock<boost::mutex> lock(this->_mutex);
				this->_condition.wait(lock, boost::bind(&future_body<T>::check_if_complete, this, &lock));
				if(this->_exception)
				{
					rethrow_exception(this->_exception);
				}
				BOOST_ASSERT(_value);
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				boost::unique_lock<boost::mutex> lock(this->_mutex);
				return this->_condition.timed_wait(lock, absolute_time, boost::bind(&future_body<T>::check_if_complete, this, &lock));
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{
				bool emitSignal = false;
				{
					boost::unique_lock<boost::mutex> lock(this->_mutex);
					if(this->_exception == false && !_value)
					{
						emitSignal = true;
						this->_condition.notify_all();
						this->_exception = exp;
					}
				}
				if(emitSignal)
				{
					this->_updateSignal();
				}
			}
			virtual exception_ptr get_exception_ptr() const
			{
				boost::unique_lock<boost::mutex> lock(this->_mutex);
				return this->_exception;
			}
			virtual waiter_event_queue& waiter_callbacks() const
			{
				_waiter_callbacks.set_owner(this->shared_from_this());
				return _waiter_callbacks;
			}
		private:
			bool check_if_complete(boost::unique_lock<boost::mutex> *lock) const
			{
				// do initial check to make sure we don't run any wait callbacks if we are already complete
				const bool complete = _value || this->_exception;
				if(complete) return complete;

				lock->unlock();
				_waiter_callbacks.poll();
				lock->lock();
				return _value || this->_exception;
			}

			boost::optional<T> _value;
			mutable waiter_event_queue _waiter_callbacks;
		};

		template<typename ProxyType, typename ActualType>
		static ProxyType default_conversion_function(const ActualType& actualValue)
		{
			return ProxyType(actualValue);
		}
		template<typename ActualType>
		static null_type null_conversion_function(const ActualType& actualValue)
		{
			return null_type();
		}

		/* class which monitors another future_body_base<ActualType>, while returning values of type ProxyType.
		Allows for implicit and explicit conversions between Futures with different template types.
		*/
		template <typename ProxyType, typename ActualType> class future_body_proxy:
			public future_body_base<ProxyType>
		{
		public:
			// static factory function
			static boost::shared_ptr<future_body_proxy> create(
				const boost::shared_ptr<future_body_base<ActualType> > &actualFutureBody,
				const boost::function<ProxyType (const ActualType&)> &conversionFunction)
			{
				boost::shared_ptr<future_body_proxy> new_object(new future_body_proxy(actualFutureBody, conversionFunction));

				new_object->_waiter_callbacks.set_owner(new_object);

				typedef typename future_body_untyped_base::update_slot_type slot_type;
				new_object->_actualFutureBody->connectUpdate(
					slot_type(&future_body_proxy::handle_actual_body_complete, new_object.get()).track(new_object));
				if(actualFutureBody->ready() || actualFutureBody->get_exception_ptr())
				{
					new_object->handle_actual_body_complete();
				}

				typedef typename waiter_event_queue::slot_type event_slot_type;
				actualFutureBody->waiter_callbacks().connect_slot(event_slot_type(
					&waiter_event_queue::post<event_queue::event_type>, &new_object->waiter_callbacks(), _1).
					track(new_object));
				// deal with any events already in _actualFutureBody's event queue
				new_object->waiter_callbacks().post(actualFutureBody->waiter_callbacks().create_poll_event());
				return new_object;
			}
			virtual void setValue(const ProxyType &value)
			{
				std::ostringstream message;
				message << __FUNCTION__ << ": didn't ever expect this function to actually get called!";
				throw std::invalid_argument(message.str());
			}
			virtual bool ready() const
			{
				boost::unique_lock<boost::mutex> lock(this->_mutex);
				return _proxyValue;
			}
			virtual const ProxyType& getValue() const
			{
				_actualFutureBody->join();
				boost::unique_lock<boost::mutex> lock(this->_mutex);
				this->_condition.wait(lock, boost::bind(&future_body_proxy::check_if_complete, this, &lock));
				if(this->_exception) rethrow_exception(this->_exception);
				BOOST_ASSERT(_proxyValue);
				return _proxyValue.get();
			}
			virtual void join() const
			{
				_actualFutureBody->join();
				boost::unique_lock<boost::mutex> lock(this->_mutex);
				this->_condition.wait(lock, boost::bind(&future_body_proxy::check_if_complete, this, &lock));
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				if(_actualFutureBody->timed_join(absolute_time) == false) return false;
				boost::unique_lock<boost::mutex> lock(this->_mutex);
				return this->_condition.timed_wait(lock, absolute_time, boost::bind(&future_body_proxy::check_if_complete, this, &lock));
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{
				_actualFutureBody->cancel(exp);
				boost::unique_lock<boost::mutex> lock(this->_mutex);
				this->_condition.notify_all();
			}
			virtual exception_ptr get_exception_ptr() const
			{
				boost::unique_lock<boost::mutex> lock(this->_mutex);
				return this->_exception;
			}
			virtual waiter_event_queue& waiter_callbacks() const
			{
				return _waiter_callbacks;
			}
		private:
			future_body_proxy(boost::shared_ptr<future_body_base<ActualType> > actualFutureBody,
				const boost::function<ProxyType (const ActualType&)> &conversionFunction):
				_actualFutureBody(actualFutureBody),
				_conversionFunction(conversionFunction),
				_waiter_callbacks(this->_mutex, this->_condition),
				_conversionEventPosted(false)
			{}

			void waiter_event()
			{
				boost::optional<ProxyType> value;
				try
				{
					value = _conversionFunction(_actualFutureBody->getValue());
				}catch(...)
				{
					{
						boost::unique_lock<boost::mutex> lock(this->_mutex);
						this->_exception = current_exception();
					}
					this->_updateSignal();
					return;
				}
				{
					boost::unique_lock<boost::mutex> lock(this->_mutex);
					BOOST_ASSERT(!_proxyValue);
					_proxyValue = value;
				}
				this->_updateSignal();
			}
			void handle_actual_body_complete()
			{
				{
					boost::unique_lock<boost::mutex> lock(this->_mutex);
					if(_conversionEventPosted) return;
					_conversionEventPosted = true;
				}
				_waiter_callbacks.post(boost::bind(&future_body_proxy::waiter_event, this));
			}
			bool check_if_complete(boost::unique_lock<boost::mutex> *lock) const
			{
				// do initial check to make sure we don't run any wait callbacks if we are already complete
				const bool complete =  _proxyValue || this->_exception;
				if(complete) return complete;

				lock->unlock();
				_waiter_callbacks.poll();
				lock->lock();
				return _proxyValue || this->_exception;
			}

			boost::shared_ptr<future_body_base<ActualType> > _actualFutureBody;
			boost::function<ProxyType (const ActualType&)> _conversionFunction;
			mutable boost::optional<ProxyType> _proxyValue;
			mutable waiter_event_queue _waiter_callbacks;
			mutable bool _conversionEventPosted;
		};

		template <typename T> class promise_body
		{
		public:
			promise_body(): _future_body(new future_body<T>())
			{}
			~promise_body()
			{
				renege(uncertain_future());
			}

			void fulfill(const T &value)
			{
				_future_body->setValue(value);
			}
			template <typename E>
			void renege(const E &exception)
			{
				_future_body->cancel(poet::copy_exception(exception));
			}
			void renege(const exception_ptr &exp)
			{
				_future_body->cancel(exp);
			}
			inline void handle_future_fulfillment(const future<T> &future_value);
			inline void handle_future_void_fulfillment(const future<void> &future_value);
			bool has_future() const
			{
				return !_future_body.unique();
			}

			boost::shared_ptr<future_body_base<T> > _future_body;
		};

		template<typename T>
			class nonvoid_future_body_base
		{
		public:
			typedef future_body_base<T> type;
		};
		template<>
			class nonvoid_future_body_base<void>
		{
		public:
			typedef future_body_untyped_base type;
		};

		template<typename T>
			future<T> create_future(const boost::shared_ptr<future_body_untyped_base> &body);
		template<>
			future<void> create_future<void>(const boost::shared_ptr<future_body_untyped_base> &body);
		template<typename T>
			const boost::shared_ptr<typename nonvoid_future_body_base<T>::type>& get_future_body(const poet::future<T> &f);
	} // namespace detail

	template <typename T>
	class promise
	{
	public:
		template <typename U>
		friend class future;
		template <typename U>
		friend class promise;
		friend class promise<void>;

		typedef T value_type;
		promise(): _pimpl(new detail::promise_body<T>)
		{}
		virtual ~promise() {}
		void fulfill(const T &value)
		{
			_pimpl->fulfill(value);
		}
		void fulfill(const future<T> &future_value)
		{
			typedef typename detail::future_body_untyped_base::update_slot_type slot_type;
			detail::get_future_body(future_value)->connectUpdate(
				slot_type(&detail::promise_body<T>::handle_future_fulfillment, _pimpl.get(), future_value).track(_pimpl));
		}
		template <typename E>
		void renege(const E &exception)
		{
			_pimpl->renege(exception);
		}
		void renege(const poet::exception_ptr &exp)
		{
			_pimpl->renege(exp);
		}
		bool has_future() const
		{
			return _pimpl->has_future();
		}
		void reset()
		{
			promise temp;
			swap(temp);
		}
		void swap(promise &other)
		{
			using std::swap;
			swap(_pimpl, other._pimpl);
		}
	private:
		boost::shared_ptr<detail::promise_body<T> > _pimpl;
	};

	// void specialization
	template<>
	class promise<void>: private promise<detail::nonvoid<void>::type>
	{
	private:
		typedef promise<detail::nonvoid<void>::type> base_type;
	public:
		template <typename U>
		friend class future;

		typedef void value_type;

		promise()
		{}
		promise(const promise<void> &other): base_type(other)
		{}
		// allow conversion from a promise with any template type to a promise<void>
		template <typename OtherType>
		promise(const promise<OtherType> &other)
		{
			boost::function<null_type (const OtherType&)> conversion_function =
				boost::bind(&detail::null_conversion_function<OtherType>, _1);
			_pimpl->_future_body = detail::future_body_proxy<detail::nonvoid<void>::type, OtherType>::create(
				other._pimpl->_future_body, conversion_function);
		}
		virtual ~promise() {}
		void fulfill()
		{
			base_type::fulfill(null_type());
		}
		inline void fulfill(const future<void> &future_value);
		using base_type::renege;
		using base_type::reset;
		using base_type::swap;
	};

	template<typename T>
	void swap(promise<T> &a, promise<T> &b)
	{
		a.swap(b);
	}

	template <typename T> class future
	{
		friend future<T> detail::create_future<T>(const boost::shared_ptr<detail::future_body_untyped_base> &body);
		friend const boost::shared_ptr<typename detail::nonvoid_future_body_base<T>::type>& detail::get_future_body<T>(const poet::future<T> &f);
	public:
		template <typename OtherType> friend class future;
		friend class future<void>;

		typedef T value_type;

		future(const promise<T> &promise): _future_body(promise._pimpl->_future_body)
		{}
		template <typename OtherType>
		future(const promise<OtherType> &promise)
		{
			future<OtherType> other_future(promise);
			*this = other_future;
		}
		future(const T &value): _future_body(new detail::future_body<T>(value))
		{}
		template <typename OtherType> future(const future<OtherType> &other)
		{
			BOOST_ASSERT(typeid(T) != typeid(OtherType));
			if(other._future_body == 0)
			{
				_future_body.reset();
				return;
			}
			boost::function<T (const OtherType&)> typedConversionFunction =
				boost::bind(&detail::default_conversion_function<T, OtherType>, _1);
			_future_body = detail::future_body_proxy<T, OtherType>::create(
				other._future_body, typedConversionFunction);
		}
		future()
		{}
		virtual ~future() {}
		bool ready() const
		{
			if(_future_body == 0) return false;
			bool result = _future_body->ready();
			if(result == true) return result;
			_future_body->waiter_callbacks().poll();
			return _future_body->ready();
		}
		const T& get() const
		{
			if(_future_body == 0)
			{
				throw uncertain_future();
			}
			return _future_body->getValue();
		}
		operator const T&() const
		{
			return get();
		}
		void join() const
		{
			if(_future_body == 0)
			{
				return;
			}
			_future_body->join();
		}
		bool timed_join(const boost::system_time &absolute_time) const
		{
			if(_future_body == false) return true;
			return _future_body->timed_join(absolute_time);
		}
		template <typename OtherType> const future<T>& operator=(const future<OtherType> &other)
		{
			BOOST_ASSERT(typeid(T) != typeid(OtherType));
			_future_body = detail::future_body_proxy<T, OtherType>::create(other._future_body);
			return *this;
		}
		bool has_exception() const
		{
			if(_future_body == 0) return true;
			bool result = _future_body->get_exception_ptr();
			if(result == true) return result;
			_future_body->waiter_callbacks().poll();
			return _future_body->get_exception_ptr();
		}
	private:
		future(const boost::shared_ptr<detail::future_body_base<T> > &future_body):_future_body(future_body)
		{}

		boost::shared_ptr<detail::future_body_base<T> > _future_body;
	};

	template <>
	class future<void>
	{
		friend future<void> detail::create_future<void>(const boost::shared_ptr<detail::future_body_untyped_base> &body);
		friend const boost::shared_ptr<detail::nonvoid_future_body_base<void>::type>& detail::get_future_body<void>(const poet::future<void> &f);
	public:
		template <typename OtherType> friend class future;
		friend class promise<void>;

		typedef void value_type;

		future(const promise<void> &promise): _future_body(promise._pimpl->_future_body)
		{}
		template <typename OtherType>
		future(const promise<OtherType> &promise)
		{
			future<OtherType> other_future(promise);
			*this = other_future;
		}
		template <typename OtherType> future(const future<OtherType> &other)
		{
			BOOST_ASSERT(typeid(void) != typeid(OtherType));
			if(other._future_body == 0)
			{
				_future_body.reset();
				return;
			}
			_future_body = other._future_body;
		}
		future()
		{}
		virtual ~future() {}
		void get() const
		{
			if(_future_body == 0)
			{
				throw uncertain_future();
			}
			_future_body->join();
		}
		operator void () const
		{
			get();
		}
		template <typename OtherType> const future<void>& operator=(const future<OtherType> &other)
		{
			BOOST_ASSERT(typeid(void) != typeid(OtherType));
			_future_body = other._future_body;
			return *this;
		}
		void join() const
		{
			if(_future_body == 0)
			{
				return;
			}
			_future_body->join();
		}
		bool timed_join(const boost::system_time &absolute_time) const
		{
			if(_future_body == 0)
			{
				return true;
			}
			return _future_body->timed_join(absolute_time);
		}
		bool ready() const
		{
			if(_future_body == 0) return false;
			_future_body->waiter_callbacks().poll();
			return _future_body->ready();
		}
		bool has_exception() const
		{
			if(_future_body == 0) return true;
			_future_body->waiter_callbacks().poll();
			return _future_body->get_exception_ptr();
		}
	private:
		future(const boost::shared_ptr<detail::future_body_untyped_base > &future_body):_future_body(future_body)
		{}

		boost::shared_ptr<detail::future_body_untyped_base > _future_body;
	};

	namespace detail
	{
		template <typename T>
		void promise_body<T>::handle_future_fulfillment(const future<T> &future_value)
		{
			try
			{
				fulfill(future_value.get());
			}
			catch(...)
			{
				renege(current_exception());
			}
		}

		template<typename T>
		void promise_body<T>::handle_future_void_fulfillment(const future<void> &future_value)
		{
			try
			{
				future_value.get();
				fulfill(null_type());
			}
			catch(...)
			{
				renege(current_exception());
			}
		}

		template<typename T>
			future<T> create_future(const boost::shared_ptr<future_body_untyped_base> &body)
		{
			using boost::dynamic_pointer_cast;
			return future<T>(dynamic_pointer_cast<future_body_base<T> >(body));
		}
		template<>
			future<void> create_future<void>(const boost::shared_ptr<future_body_untyped_base> &body)
		{
			return future<void>(body);
		}

		template<typename T>
			class shared_uncertain_future_body
		{
		public:
			static boost::shared_ptr<typename nonvoid_future_body_base<T>::type> value;
		};
		template<typename T>
			boost::shared_ptr<typename nonvoid_future_body_base<T>::type>
				shared_uncertain_future_body<T>::value(
					new future_body<typename nonvoid<T>::type>(
						copy_exception(uncertain_future()), 0));

		template<typename T>
			const boost::shared_ptr<typename nonvoid_future_body_base<T>::type>& get_future_body(const poet::future<T> &f)
		{
			if(!f._future_body) return shared_uncertain_future_body<T>::value;
			return f._future_body;
		}
	} // namespace detail

	void promise<void>::fulfill(const future<void> &future_value)
	{
		typedef detail::future_body_untyped_base::update_slot_type slot_type;
		detail::get_future_body(future_value)->connectUpdate(slot_type(&detail::promise_body<detail::nonvoid<void>::type>::handle_future_void_fulfillment,
			_pimpl.get(), future_value).track(_pimpl));
	}
}


#endif // _POET_FUTURE_H
