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
#include <boost/signals2/signal.hpp>
#include <boost/weak_ptr.hpp>
#include <poet/detail/condition.hpp>
#include <poet/detail/event_queue.hpp>
#include <poet/detail/nonvoid.hpp>
#include <poet/detail/utility.hpp>
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
		// forward declarations
		class future_body_untyped_base;
		template<typename T>
			class future_body_base;
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
			const boost::shared_ptr<typename nonvoid_future_body_base<T>::type>& get_future_body(const poet::future<T> &f);

		/* class for holding wait callbacks.  Any thread can post a functor to the waiter_event_queue,
		but only future-waiting threads should pop them off and execute them. */
		class waiter_event_queue
		{
			typedef boost::signals2::signal<void (const event_queue::event_type &)> event_posted_type;
			typedef event_posted_type::slot_type slot_type;
			typedef std::vector<boost::signals2::connection> connections_type;
		public:
			waiter_event_queue(boost::mutex &condition_mutex, boost::condition &condition):
				_condition_mutex(condition_mutex), _condition(condition), _posting_closed(false)
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
				{
					boost::unique_lock<boost::mutex> lock(_mutex);

					if(_posting_closed) return;
				}
				_events.post(event);
				_event_posted(create_poll_event());
				{
					boost::unique_lock<boost::mutex> lock(_condition_mutex);
					_condition.notify_all();
				}
			}
			void poll()
			{
				_events.poll();
			}
			boost::signals2::connection observe(waiter_event_queue &other)
			{
				boost::signals2::connection connection;
				{
					boost::unique_lock<boost::mutex> lock(_mutex);

					if(_posting_closed) return connection;

					slot_type slot(&waiter_event_queue::post<event_queue::event_type>, this, _1);
					BOOST_ASSERT(_weak_this.expired() == false);
					slot.track(_weak_this);
					connection = other._event_posted.connect(slot);
					_connections.push_back(connection);
				}
				post(other.create_poll_event());
				return connection;
			}
			void close_posting()
			{
				boost::unique_lock<boost::mutex> lock(_mutex);

				_posting_closed = true;

				connections_type::iterator it;
				for(it = _connections.begin(); it != _connections.end(); ++it)
				{
					it->disconnect();
				}
			}
		private:
			event_queue::event_type create_poll_event() const
			{
				BOOST_ASSERT(_weak_this.expired() == false);
				event_queue::event_type event = boost::bind(&waiter_event_queue::poll_event_impl, _weak_this);
				return event;
			}
			static void poll_event_impl(const boost::weak_ptr<waiter_event_queue> &weak_this)
			{
				boost::shared_ptr<waiter_event_queue> shared_this = weak_this.lock();
				if(!shared_this) return;
				shared_this->poll();
			}

			event_queue _events;
			boost::mutex _mutex;
			boost::mutex &_condition_mutex;
			boost::condition &_condition;
			event_posted_type _event_posted;
			boost::weak_ptr<waiter_event_queue> _weak_this;
			connections_type _connections;
			bool _posting_closed;
		};

		class future_body_untyped_base: public boost::enable_shared_from_this<future_body_untyped_base>
		{
		public:
			typedef boost::signals2::signal<void ()> update_signal_type;
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
			boost::signals2::connection connectUpdate(const update_signal_type::slot_type &slot)
			{
				return _updateSignal.connect(slot);
			}
			boost::mutex & mutex() const
			{
				return _mutex;
			}
			boost::condition & condition() const
			{
				return _condition;
			}
		protected:
			update_signal_type _updateSignal;
			mutable poet::exception_ptr _exception;
		private:
			mutable boost::mutex _mutex;
			mutable boost::condition _condition;
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
			static boost::shared_ptr<future_body> create()
			{
				boost::shared_ptr<future_body> new_object(new future_body);
				new_object->_waiter_callbacks.set_owner(new_object);
				return new_object;
			}
			static boost::shared_ptr<future_body> create(const T &value)
			{
				boost::shared_ptr<future_body> new_object(new future_body(value));
				new_object->_waiter_callbacks.set_owner(new_object);
				return new_object;
			}

			virtual ~future_body() {}
			virtual void setValue(const T &value)
			{
				bool emit_signal = false;
				{
					boost::unique_lock<boost::mutex> lock(this->mutex());
					if(this->_exception == false && !_value)
					{
						_value = value;
						this->condition().notify_all();
						emit_signal = true;
						clear_dependencies();
					}
				}
				if(emit_signal)
				{
					this->_updateSignal();
				}
			}
			virtual bool ready() const
			{
					boost::unique_lock<boost::mutex> lock(this->mutex());
				return _value;
			}
			virtual const T& getValue() const
			{
				boost::unique_lock<boost::mutex> lock(this->mutex());
				this->condition().wait(lock, boost::bind(&future_body<T>::check_if_complete, this, &lock));
				if(this->_exception)
				{
					rethrow_exception(this->_exception);
				}
				BOOST_ASSERT(_value);
				return _value.get();
			}
			virtual void join() const
			{
				boost::unique_lock<boost::mutex> lock(this->mutex());
				this->condition().wait(lock, boost::bind(&future_body<T>::check_if_complete, this, &lock));
				if(this->_exception)
				{
					rethrow_exception(this->_exception);
				}
				BOOST_ASSERT(_value);
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				boost::unique_lock<boost::mutex> lock(this->mutex());
				return this->condition().timed_wait(lock, absolute_time, boost::bind(&future_body<T>::check_if_complete, this, &lock));
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{
				bool emitSignal = false;
				{
					boost::unique_lock<boost::mutex> lock(this->mutex());
					if(this->_exception == false && !_value)
					{
						emitSignal = true;
						this->condition().notify_all();
						this->_exception = exp;
						clear_dependencies();
					}
				}
				if(emitSignal)
				{
					this->_updateSignal();
				}
			}
			virtual exception_ptr get_exception_ptr() const
			{
				boost::unique_lock<boost::mutex> lock(this->mutex());
				return this->_exception;
			}
			virtual waiter_event_queue& waiter_callbacks() const
			{
				return _waiter_callbacks;
			}
			void add_dependency(const boost::shared_ptr<void> &dependency)
			{
				boost::unique_lock<boost::mutex> lock(this->mutex());
				if(_value || this->_exception) return;
				_dependencies.push_back(dependency);
			}
		private:
			future_body(): _waiter_callbacks(future_body_untyped_base::mutex(), future_body_untyped_base::condition())
			{}
			future_body(const T &value): _value(value),
				_waiter_callbacks(future_body_untyped_base::mutex(), future_body_untyped_base::condition())
			{}

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
			void clear_dependencies()
			{
				std::vector<boost::shared_ptr<void> >().swap(_dependencies);
				_waiter_callbacks.close_posting();
			}

			boost::optional<T> _value;
			mutable waiter_event_queue _waiter_callbacks;
			std::vector<boost::shared_ptr<void> > _dependencies;
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
				slot_type update_slot(&future_body_proxy::handle_actual_body_complete, new_object.get());
				update_slot.track(new_object);
				boost::signals2::connection conn;
				conn = new_object->_actualFutureBody->connectUpdate(update_slot);
				if(actualFutureBody->ready() || actualFutureBody->get_exception_ptr())
				{
					try
					{
						update_slot();
					}
					catch(const boost::signals2::expired_slot &)
					{
						conn.disconnect();
					}
					/* we don't need to bother observing actualFutureBody's waiter_event_queue
					if it was already complete */
					return new_object;
				}

				new_object->waiter_callbacks().observe(actualFutureBody->waiter_callbacks());

				return new_object;
			}
			virtual void setValue(const ProxyType &value)
			{
				BOOST_ASSERT(false);
			}
			virtual bool ready() const
			{
				boost::unique_lock<boost::mutex> lock(this->mutex());
				return _proxyValue;
			}
			virtual const ProxyType& getValue() const
			{
				_actualFutureBody->join();
				boost::unique_lock<boost::mutex> lock(this->mutex());
				this->condition().wait(lock, boost::bind(&future_body_proxy::check_if_complete, this, &lock));
				if(this->_exception) rethrow_exception(this->_exception);
				BOOST_ASSERT(_proxyValue);
				return _proxyValue.get();
			}
			virtual void join() const
			{
				_actualFutureBody->join();
				boost::unique_lock<boost::mutex> lock(this->mutex());
				this->condition().wait(lock, boost::bind(&future_body_proxy::check_if_complete, this, &lock));
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				if(_actualFutureBody->timed_join(absolute_time) == false) return false;
				boost::unique_lock<boost::mutex> lock(this->mutex());
				return this->condition().timed_wait(lock, absolute_time, boost::bind(&future_body_proxy::check_if_complete, this, &lock));
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{
				_actualFutureBody->cancel(exp);
				boost::unique_lock<boost::mutex> lock(this->mutex());
				this->condition().notify_all();
			}
			virtual exception_ptr get_exception_ptr() const
			{
				boost::unique_lock<boost::mutex> lock(this->mutex());
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
				_waiter_callbacks(future_body_untyped_base::mutex(), future_body_untyped_base::condition()),
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
						boost::unique_lock<boost::mutex> lock(this->mutex());
						this->_exception = current_exception();
					}
					this->_updateSignal();
					return;
				}
				{
					boost::unique_lock<boost::mutex> lock(this->mutex());
					BOOST_ASSERT(!_proxyValue);
					_proxyValue = value;
				}
				this->_updateSignal();
			}
			void handle_actual_body_complete()
			{
				{
					boost::unique_lock<boost::mutex> lock(this->mutex());
					if(_conversionEventPosted) return;
					_conversionEventPosted = true;
				}
				_waiter_callbacks.post(boost::bind(&future_body_proxy::waiter_event, this));
				_waiter_callbacks.close_posting();
				throw boost::signals2::expired_slot();
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

		class promise_body_untyped_base
		{
		public:
			virtual ~promise_body_untyped_base()
			{}
			virtual void renege(const exception_ptr &exp) = 0;
			virtual bool has_future() const = 0;
			virtual boost::shared_ptr<future_body_untyped_base> shared_future_body() const = 0;
		};

		template <typename T> class promise_body: public promise_body_untyped_base
		{
		public:
			promise_body(): _future_body(future_body<T>::create())
			{}
			~promise_body()
			{
				renege(copy_exception(uncertain_future()));
			}

			void fulfill(const T &value)
			{
				_future_body->setValue(value);
			}
			void future_fulfill(const future<T> &future_value)
			{
				typedef typename future_body_untyped_base::update_slot_type slot_type;
				slot_type update_slot(&promise_body::handle_future_fulfillment,
					_future_body.get(), get_future_body(future_value).get());
				update_slot.track(_future_body);
				update_slot.track(get_future_body(future_value));
				future_fulfill_guts(get_future_body(future_value), update_slot);
			}

			// provided for promise<void>
			void future_void_fulfill(const future<void> &future_value)
			{
				typedef typename future_body_untyped_base::update_slot_type slot_type;
				slot_type update_slot(&promise_body::handle_future_void_fulfillment,
					_future_body.get(), get_future_body(future_value).get());
				update_slot.track(_future_body);
				update_slot.track(get_future_body(future_value));
				future_fulfill_guts(get_future_body(future_value), update_slot);
			}

			virtual void renege(const exception_ptr &exp)
			{
				_future_body->cancel(exp);
			}
			virtual bool has_future() const
			{
				return !_future_body.unique();
			}
			virtual boost::shared_ptr<future_body_untyped_base> shared_future_body() const
			{
				return _future_body;
			}

			boost::shared_ptr<future_body<T> > _future_body;
		private:
			template<typename U>
				void future_fulfill_guts(const boost::shared_ptr<U> &fulfiller_body,
				const future_body_untyped_base::update_slot_type &update_slot)
			{
				boost::signals2::connection conn;
				conn = fulfiller_body->connectUpdate(update_slot);
				if(fulfiller_body->ready() || fulfiller_body->get_exception_ptr())
				{
					try
					{
						update_slot();
					}
					catch(const boost::signals2::expired_slot &)
					{
						conn.disconnect();
					}
					/* if fulfiller_body was already complete, we are finished. */
					return;
				}

				_future_body->waiter_callbacks().observe(fulfiller_body->waiter_callbacks());

				// stick shared_ptr to future_value in dependent _future_body
				_future_body->add_dependency(fulfiller_body);
			}
			inline static void handle_future_fulfillment(future_body_base<T> *fulfillee,
				future_body_base<T> *fulfiller);
			inline static void handle_future_void_fulfillment(future_body_base<nonvoid<void>::type> *fulfillee,
				future_body_untyped_base *fulfiller);
		};

		template<typename T>
			future<T> create_future(const boost::shared_ptr<future_body_untyped_base> &body);
		template<>
			future<void> create_future<void>(const boost::shared_ptr<future_body_untyped_base> &body);
	} // namespace detail

	template <typename T>
	class promise
	{
		template <typename U>
		friend class future;
		template <typename U>
		friend class promise;
		friend class promise<void>;
	public:
		typedef T value_type;
		promise(): _pimpl(new detail::promise_body<T>)
		{}
		virtual ~promise() {}
		template<typename U>
			void fulfill(const U &value)
		{
			_pimpl->fulfill(value);
		}
		template<typename U>
			void fulfill(const future<U> &future_value)
		{
			_pimpl->future_fulfill(future_value);
		}
		template <typename E>
		void renege(const E &exception)
		{
			_pimpl->renege(poet::copy_exception(exception));
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
	class promise<void>
	{
		template <typename U>
		friend class future;
		template <typename U>
		friend class promise;
	public:
		typedef void value_type;

		promise(): _pimpl(new detail::promise_body<detail::nonvoid<void>::type>)
		{}
		// allow conversion from a promise with any template type to a promise<void>
		template <typename OtherType>
		promise(const promise<OtherType> &other): _pimpl(other._pimpl)
		{}
		virtual ~promise() {}
		void fulfill()
		{
			downcast_pimpl()->fulfill(detail::nonvoid<void>::type());
		}
		void fulfill(const future<void> &future_value)
		{
			downcast_pimpl()->future_void_fulfill(future_value);
		}
		template <typename E>
		void renege(const E &exception)
		{
			_pimpl->renege(poet::copy_exception(exception));
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
		boost::shared_ptr<detail::promise_body<detail::nonvoid<void>::type> > downcast_pimpl() const
		{
			using boost::dynamic_pointer_cast;
			boost::shared_ptr<detail::promise_body<detail::nonvoid<void>::type> > downcast =
				dynamic_pointer_cast<detail::promise_body<detail::nonvoid<void>::type> >(_pimpl);
			if(downcast == false)
			{
				throw std::invalid_argument("Cannot fulfill a promise<T> for non-void T through a promise<void>");
			}
			return downcast;
		}

		boost::shared_ptr<detail::promise_body_untyped_base> _pimpl;
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

		typedef boost::shared_ptr<detail::future_body_base<T> > future_body_type;
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
		future(const T &value): _future_body(detail::future_body<T>::create(value))
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
		template <typename OtherType>
			future(const OtherType &other): _future_body(detail::future_body<T>::create(other))
		{}
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
		void swap(future &other)
		{
			using std::swap;
			swap(_future_body, other._future_body);
		}
	private:
		future(const boost::shared_ptr<detail::future_body_base<T> > &future_body):_future_body(future_body)
		{}

		future_body_type _future_body;
	};

	template <>
	class future<void>
	{
		friend future<void> detail::create_future<void>(const boost::shared_ptr<detail::future_body_untyped_base> &body);
		friend const boost::shared_ptr<detail::nonvoid_future_body_base<void>::type>& detail::get_future_body<void>(const poet::future<void> &f);

		typedef boost::shared_ptr<detail::future_body_untyped_base > future_body_type;
	public:
		template <typename OtherType> friend class future;
		friend class promise<void>;

		typedef void value_type;

		future(const promise<void> &promise): _future_body(promise._pimpl->shared_future_body())
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
			exception_ptr ep = _future_body->get_exception_ptr();
			if(ep) rethrow_exception(ep);
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
		void swap(future &other)
		{
			using std::swap;
			swap(_future_body, other._future_body);
		}
	private:
		future(const boost::shared_ptr<detail::future_body_untyped_base > &future_body):_future_body(future_body)
		{}

		future_body_type _future_body;
	};

	template<typename T>
	void swap(future<T> &a, future<T> &b)
	{
		a.swap(b);
	}

	namespace detail
	{
		template<typename T>
			void promise_body<T>::handle_future_fulfillment(future_body_base<T> *fulfillee,
			future_body_base<T> *fulfiller)
		{
			try
			{
				fulfillee->setValue(fulfiller->getValue());
			}
			catch(...)
			{
				fulfillee->cancel(current_exception());
			}
			throw boost::signals2::expired_slot();
		}
		template <typename T>
		void promise_body<T>::handle_future_void_fulfillment(future_body_base<nonvoid<void>::type> * fulfillee,
			future_body_untyped_base * fulfiller)
		{
			exception_ptr ep = fulfiller->get_exception_ptr();
			if(ep)
			{
				fulfillee->cancel(ep);
			}else
			{
				BOOST_ASSERT(fulfiller->ready());
				fulfillee->setValue(null_type());
			}
			throw boost::signals2::expired_slot();
		}

		template<typename T>
			future<T> create_future(const boost::shared_ptr<future_body_untyped_base> &body)
		{
			using boost::dynamic_pointer_cast;
			return future<T>(dynamic_pointer_cast<future_body_base<T> >(body));
		}
		template<>
			inline future<void> create_future<void>(const boost::shared_ptr<future_body_untyped_base> &body)
		{
			return future<void>(body);
		}

		template<typename T>
			class shared_uncertain_future_body
		{
		public:
			static boost::shared_ptr<typename nonvoid_future_body_base<T>::type> value;
		private:
			static boost::shared_ptr<typename nonvoid_future_body_base<T>::type> create()
			{
				boost::shared_ptr<typename nonvoid_future_body_base<T>::type> new_object(
					future_body<typename nonvoid<T>::type>::create());
				new_object->cancel(copy_exception(uncertain_future()));
				return new_object;
			}
		};
		template<typename T>
			boost::shared_ptr<typename nonvoid_future_body_base<T>::type>
				shared_uncertain_future_body<T>::value(shared_uncertain_future_body<T>::create());

		template<typename T>
			const boost::shared_ptr<typename nonvoid_future_body_base<T>::type>& get_future_body(const poet::future<T> &f)
		{
			if(!f._future_body) return shared_uncertain_future_body<T>::value;
			return f._future_body;
		}
	} // namespace detail
}


#endif // _POET_FUTURE_H
