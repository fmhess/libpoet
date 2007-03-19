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
/* This software was developed at the National Institute of Standards and
 * Technology by employees of the Federal Government in the course of
 * their official duties. Pursuant to title 17 Section 105 of the United
 * States Code this software is not subject to copyright protection and is
 * in the public domain. This is an experimental system. NIST assumes no
 * responsibility whatsoever for its use by other parties, and makes no
 * guarantees, expressed or implied, about its quality, reliability, or
 * any other characteristic. We would appreciate acknowledgement if the
 * software is used.
 */

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
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

namespace poet
{
	template <typename T>
	class future;

	/*!  \brief Exception thrown by a cancelled future.

	This exception is thrown when an attempt to convert a future to
	its associated value fails due to future::cancel() being called
	on a future that references the same promise.
	*/
	class cancelled_future: public std::runtime_error
	{
	public:
		/*! Constructor. */
		cancelled_future(): std::runtime_error("poet::cancelled_future")
		{}
		/*! Virtual destructor. */
		virtual ~cancelled_future() throw() {}
	};
	/*!  \brief Exception thrown by an uncertain future.

	This exception is thrown when an attempt is made to convert a
	future with no promise into its associated value.  This
	can happen if the future was default-constructed, or
	its associated promise object has been destroyed without
	being fulfilled.
	*/
	class uncertain_future: public std::runtime_error
	{
	public:
		/*! Constructor. */
		uncertain_future(): std::runtime_error("poet::uncertain_future")
		{}
		/*! Virtual destructor. */
		virtual ~uncertain_future() throw() {}
	};

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
				if(emitSignal) this->_updateSignal();
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

	Promise objects are handles with shallow copy semantics.
	*/
	template <typename T>
	class promise
	{
	public:
		template <typename U>
		friend class future;

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
	private:
		boost::shared_ptr<detail::promise_body<T> > _pimpl;
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
		/*! T */
		typedef T value_type;
		/*! boost::signal<void ()>::slot_type */
		typedef typename detail::future_body_base<T>::update_signal_type::slot_type update_slot_type;

		/*! Creates a new future from a promise.  When the promise referenced by <em>promise</em>
		is fulfilled or broken, the future will become ready.  */
		future(const promise<T> &promise): _future_body(promise._pimpl->_future_body)
		{}
		/*! Creates a new future from a promise with a template type <em>OtherType</em> that is
		implicitly convertible to the future's value_type.  When the promise referenced by <em>promise</em>
		is fulfilled or broken, the future will become ready. */
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
		/*! The conversion operator is used to obtain an initialized value from a future.
		If the future is not ready, the conversion
		operator will block until the future's value is initialized, or
		the future's promise is broken.
		\exception cancelled_future if the conversion fails due to
		cancellation.
		\exception unspecified if the future's promise is broken, the conversion operator
		will throw whatever exception was specified by the promise::renege call.
		*/
		operator const T&() const
		{
			if(_future_body == 0)
			{
				throw uncertain_future();
			}
			return _future_body->getValue();
		}
		/*! Assignment from a future<U> is supported if U is implicitly convertible to T.
		The assignment happens immediately, and does not block waiting for <em>other</em> to become ready. */
		template <typename OtherType> const future<T>& operator =(const future<OtherType> &other)
		{
			BOOST_ASSERT(typeid(T) != typeid(OtherType));
			_future_body.reset(new detail::future_body_proxy<T, OtherType>(other._future_body));
		}
		/*! Connect a slot to be run when the future's status changes, either because
		its value is ready or its promise has been broken.
		*/
		boost::signalslib::connection connect_update(const update_slot_type &slot) const
		{
			if(_future_body == 0) throw std::invalid_argument("Future doesn't refer to any value yet. Cannot connect slot.");
			return _future_body->connectUpdate(slot);
		}
		/*! Cancel a future by breaking its promise with a cancelled_future exception. */
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

#endif // _POET_FUTURE_H
