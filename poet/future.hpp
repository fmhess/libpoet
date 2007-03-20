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
#include <boost/optional.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread_safe_signal.hpp>
#include <poet/detail/condition.hpp>
#include <poet/exception_ptr.hpp>
#include <poet/exceptions.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

namespace poet
{
	template <typename T>
	class future;

	namespace detail
	{
		template <typename T> class future_body_base
		{
		public:
			typedef typename boost::signal<void ()> update_signal_type;

			future_body_base()
			{}
			virtual ~future_body_base() {}
			virtual bool ready() const = 0;
			virtual const T& getValue() const = 0;
			virtual void setValue(const T &value) = 0;
			virtual void cancel(const poet::exception_ptr &) = 0;
			virtual bool has_exception() const = 0;
			boost::signalslib::connection connectUpdate(const update_signal_type::slot_type &slot)
			{
				return _updateSignal.connect(slot);
			}
		protected:
			update_signal_type _updateSignal;
		};

		template <typename T> class future_body: public future_body_base<T>
		{
		public:
			future_body()
			{}
			future_body(const T &value): _value(value)
			{}
			virtual ~future_body() {}
			virtual void setValue(const T &value)
			{
				bool emit_signal = false;
				{
					boost::mutex::scoped_lock lock(_mutex);
					if(_exception == 0 && _value == false)
					{
						_value = value;
						emit_signal = true;
					}
				}
				if(emit_signal)
				{
					_readyCondition.locking_notify_all();
					this->_updateSignal();
				}
			}
			virtual bool ready() const
			{
					boost::mutex::scoped_lock lock(_mutex);
				return _value;
			}
			virtual const T& getValue() const
			{
				_readyCondition.locking_wait(boost::bind(&poet::detail::future_body<T>::readyOrCancelled, this));
				boost::mutex::scoped_lock lock(_mutex);
				if(_exception)
				{
					rethrow_exception(_exception);
				}
				BOOST_ASSERT(_value);
				return _value.get();
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{
				bool emitSignal = false;
				{
					boost::mutex::scoped_lock lock(_mutex);
					if(_exception == 0 && _value == false)
					{
						emitSignal = true;
						_exception = exp;
					}
				}
				if(emitSignal)
				{
					_readyCondition.locking_notify_all();
					this->_updateSignal();
				}
			}
			virtual bool has_exception() const
			{
				boost::mutex::scoped_lock lock(_mutex);
				return _exception;
			}
		private:
			bool readyOrCancelled() const
			{
				boost::mutex::scoped_lock lock(_mutex);
				return _value || _exception;
			}

			boost::optional<T> _value;
			mutable boost::mutex _mutex;
			mutable typename detail::condition _readyCondition;
			poet::exception_ptr _exception;
		};

		template<typename ProxyType, typename ActualType>
		static ProxyType defaultConversionFunction(const ActualType& actualValue)
		{
			return ProxyType(actualValue);
		}
		/* class which monitors another future_body_base<ActualType>, while returning values of type ProxyType.
		Allows for implicit and explicit conversions between Futures with different template types.
		*/
		template <typename ProxyType, typename ActualType> class future_body_proxy:
			public future_body_base<ProxyType>
		{
		public:
			future_body_proxy(boost::shared_ptr<future_body_base<ActualType> > actualFutureBody,
				const boost::function<ProxyType (const ActualType&)> &conversionFunction):
				_actualFutureBody(actualFutureBody),
				_conversionFunction(conversionFunction)
			{
				_readyConnection = _actualFutureBody->connectUpdate(this->_updateSignal);
			}
			virtual ~future_body_proxy()
			{
				_readyConnection.disconnect();
			}
			virtual void setValue(const ProxyType &value)
			{
				std::ostringstream message;
				message << __FUNCTION__ << ": sorry, can't set Future's value of type_info.name()=" <<
					typeid(ActualType).name() << " through a proxy with a type_info.name()=" <<
					typeid(ProxyType).name() << " .\n" <<
					"Try the c++filt utility to convert the type_info names into a human-readable format.";
				std::cerr << message.str() << std::endl;
				throw std::runtime_error(message.str());
			}
			virtual bool ready() const
			{
				return _actualFutureBody->ready();
			}
			virtual const ProxyType& getValue() const
			{
				bool initialized = false;
				{
					boost::mutex::scoped_lock lock(_mutex);
					if(_proxyValue)
					{
						initialized = true;
					}
				}// read_lock destructs here
				if(initialized == false)
				{
					{
						boost::mutex::scoped_lock lock(_mutex);
						// make sure _proxyValue is still uninitialized after we have write lock
						if(_proxyValue == false)
						{
							_proxyValue = _conversionFunction(_actualFutureBody->getValue());
						}
					}// write_lock destructs here
				}
				boost::mutex::scoped_lock lock(_mutex);
				return _proxyValue.get();
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{
				_actualFutureBody->cancel(exp);
			}
			virtual bool has_exception() const
			{
				return _actualFutureBody->has_exception();
			}
		private:
			boost::shared_ptr<future_body_base<ActualType> > _actualFutureBody;
			boost::function<ProxyType (const ActualType&)> _conversionFunction;
			boost::signalslib::connection _readyConnection;
			mutable boost::optional<ProxyType> _proxyValue;
			mutable boost::mutex _mutex;
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

			boost::shared_ptr<future_body_base<T> > _future_body;
		};
	}

	/*! \brief A handle to a promise.

	Promises are used to construct futures and set their values when they
	become available.  You can also renege on a promise, which transports an exception
	instead of a value to any futures waiting on the promise.

	Promise objects are handles with shallow copy semantics.  Promises are
	reference-counted, which means a promise will automatically be
	reneged with an uncertain_future exception if its reference
	count drops to zero without the promise being fulfilled.

	The idea of making a seperate promise class from the future class
	was suggested by Chirtopher Kohlhoff.  The idea of reference
	counting the promise class was due to Braddock Gaskill.
	*/
	template <typename T>
	class promise
	{
	public:
		template <typename U>
		friend class future;
		friend class promise<void>;

		typedef T value_type;
		promise(): _pimpl(new detail::promise_body<T>)
		{}
		/*! Fulfill the promise by giving it a value.  All futures which reference
		this promise will become ready.  */
		void fulfill(const T &value)
		{
			_pimpl->fulfill(value);
		}
		/*! Chain the promise to another promise by giving it a future.  All futures which reference
		this promise will receive the value from <em>future_value</em> when it becomes ready.
		If the promise
		referenced by <em>future_value</em> is broken, this promise will also be broken.
		*/
		void fulfill(const future<T> &future_value)
		{
			typedef typename future<T>::update_slot_type slot_type;
			future_value.connect_update(slot_type(&detail::promise_body<T>::handle_future_fulfillment, _pimpl, future_value));
		}
		/*! Breaks the promise.  Any futures which reference the promise will throw
		a copy of <em>exception</em> when they attempt to get their value.
		*/
		template <typename E>
		void renege(const E &exception)
		{
			_pimpl->renege(exception);
		}
		/*! \overload */
		void renege(const poet::exception_ptr &exp)
		{
			_pimpl->renege(exp);
		}
	protected:
		// _pimpl is protected instead of private because g++  3.3.5 doesn't recognize promise<void> as friend
		boost::shared_ptr<detail::promise_body<T> > _pimpl;
	};

	// void specialization
	template<>
	class promise<void>: private promise<int>
	{
	private:
		typedef promise<int> base_type;
	public:
		template <typename U>
		friend class future;

		typedef void value_type;

		promise()
		{}
		void fulfill()
		{
			base_type::fulfill(0);
		}
		inline void fulfill(const future<void> &future_value);
		using base_type::renege;
	};

	/*! \brief A handle to a future value.

	Futures are wrappers around values which may not exist yet.  They are used to support asyncronous
	function calls.  Since a Future can be
	returned before any result is actually ready, an asyncronous call can return a Future
	immediately without blocking the calling thread.  The calling thread can then poll the
	future to determine when a result is ready, or block on the Future waiting
	for a result.

	Futures are handles with shallow copy semantics.  Two copies of a future will both refer to
	the same promise.
	*/
	template <typename T> class future
	{
	public:
		template <typename OtherType> friend class future;
		friend class future<void>;

		/*! T */
		typedef T value_type;
		/*! boost::signal<void ()>::slot_type */
		typedef typename detail::future_body_base<T>::update_signal_type::slot_type update_slot_type;

		/*! Creates a new future from a promise.  When the promise referenced by <em>promise</em>
		is fulfilled, the future will become ready.  */
		future(const promise<T> &promise): _future_body(promise._pimpl->_future_body)
		{}
		/*! Creates a new future from a promise with a template type <em>OtherType</em> that is
		implicitly convertible to the future's value_type.  When the promise referenced by <em>promise</em>
		is fulfilled, the future will become ready.
		*/
		template <typename OtherType>
		future(const promise<OtherType> &promise)
		{
			future<OtherType> other_future(promise);
			*this = other_future;
		}
		/*! Creates a new Future with an initialized value, and provides
		implicit conversion from a value to the corresponding Future.
		*/
		future(const T &value): _future_body(new detail::future_body<T>(value))
		{}
		/*! Creates a future from another future with a compatible template type.  The
		two Futures will both reference the same promise. */
		template <typename OtherType> future(const future<OtherType> &other)
		{
			BOOST_ASSERT(typeid(T) != typeid(OtherType));
			if(other._future_body == 0)
			{
				_future_body.reset();
				return;
			}
			boost::function<T (const OtherType&)> typedConversionFunction =
				boost::bind(&detail::defaultConversionFunction<T, OtherType>, _1);
			_future_body.reset(new detail::future_body_proxy<T, OtherType>(
				other._future_body, typedConversionFunction));
		}
		/*! Creates an uncertain future with no promise.  An attempt to get an
		uncertain future's
		value will throw an uncertain_future exception.  An uncertain future
		may gain
		promise by assigning it another future with promise.  */
		future()
		{}
		/*! Virtual destructor. */
		virtual ~future() {}
		/*! \returns true if the future's value is initialized. */
		bool ready() const
		{
			if(_future_body == 0) return false;
			return _future_body->ready();
		}
		/*!
		get() is used to obtain an initialized value from a future.
		If the future is not ready, then
		get() will block until the future's value is initialized, or
		the future's promise is broken.  The future's value may also
		be obtained without an explicit call to get(), through the
		conversion operator.
		\exception cancelled_future if the conversion fails due to
		cancellation.
		\exception unspecified if the future's promise is broken, the conversion operator
		will throw whatever exception was specified by the promise::renege call.
		\returns the future's value.
		*/
		const T& get() const
		{
			if(_future_body == 0)
			{
				throw uncertain_future();
			}
			return _future_body->getValue();
		}
		/*! The conversion operator provides implicit conversions to values.  It has
		the same effects as the explicit get() function. */
		operator const T&() const
		{
			return get();
		}
		/*! Assignment from a future<U> is supported if U is implicitly convertible to T.
		The assignment happens immediately, and does not block waiting for <em>other</em> to become ready. */
		template <typename OtherType> const future<T>& operator =(const future<OtherType> &other)
		{
			BOOST_ASSERT(typeid(T) != typeid(OtherType));
			_future_body.reset(new detail::future_body_proxy<T, OtherType>(other._future_body));
			return *this;
		}
		/*! Connect a slot to be run when the future's status changes, either because
		its value is ready or its promise has been broken.
		*/
		boost::signalslib::connection connect_update(const update_slot_type &slot) const
		{
			if(_future_body == 0) throw std::invalid_argument("Future doesn't refer to any value yet. Cannot connect slot.");
			return _future_body->connectUpdate(slot);
		}
		/*! Cancel a future by reneging on its promise with a cancelled_future exception. */
		void cancel()
		{
			_future_body->cancel(poet::copy_exception(cancelled_future()));
		}
		/*! \returns true if this future's promise has been broken.  Attempting to get the
		future's value
		will throw an exception that may give more information on why the promise was broken.
		*/
		bool has_exception() const
		{
			if(_future_body == 0) return true;
			return _future_body->has_exception();
		}
	private:
		boost::shared_ptr<detail::future_body_base<T> > _future_body;
	};

	// void specialization
	template <>
	class future<void>: private future<int>
	{
	private:
		typedef future<int> base_type;
	public:
		template <typename OtherType> friend class future;
		friend class promise<void>;

		typedef void value_type;
		typedef base_type::update_slot_type update_slot_type;

		future(const promise<void> &promise_in): base_type(reinterpret_cast<const promise<int> &>(promise_in))
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
			boost::function<void (const OtherType&)> typedConversionFunction =
				boost::bind(&detail::defaultConversionFunction<void, OtherType>, _1);
			_future_body.reset(new detail::future_body_proxy<void, OtherType>(
				other._future_body, typedConversionFunction));
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
			_future_body->getValue();
		}
		operator void () const
		{
			get();
		}
		template <typename OtherType> const future<void>& operator =(const future<OtherType> &other)
		{
			BOOST_ASSERT(typeid(void) != typeid(OtherType));
			_future_body.reset(new detail::future_body_proxy<void, OtherType>(other._future_body));
			return *this;
		}
		using base_type::ready;
		using base_type::connect_update;
		using base_type::cancel;
		using base_type::has_exception;
	};
}

template <typename T>
void poet::detail::promise_body<T>::handle_future_fulfillment(const future<T> &future_value)
{
	try
	{
		fulfill(future_value);
	}
	catch(...)
	{
		renege(current_exception());
	}
}

void poet::promise<void>::fulfill(const future<void> &future_value)
{
	typedef future<void>::update_slot_type slot_type;
	future_value.connect_update(slot_type(&detail::promise_body<int>::handle_future_fulfillment, _pimpl,
		reinterpret_cast<const future<int> &>(future_value)));
}

#endif // _POET_FUTURE_H
