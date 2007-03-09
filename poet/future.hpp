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

#include <boost/bind.hpp>
#include <cassert>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/read_write_mutex.hpp>
#include <boost/thread_safe_signal.hpp>
#include <poet/detail/condition.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <typeinfo>

namespace poet
{
	/*!  \brief Exception thrown by a cancelled future.

	This exception is thrown when an attempt to convert a future to
	its associated value fails due to future::cancel() being called
	on a future that references the same value.
	*/
	class future_cancelled: public std::runtime_error
	{
	public:
		/*! Constructor. */
		future_cancelled(): std::runtime_error("poet::future_cancelled.")
		{}
		/*! Virtual destructor. */
		virtual ~future_cancelled() throw() {}
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
			virtual void cancel() = 0;
			virtual bool cancelled() const = 0;
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
			future_body():
				_valueMutex(boost::read_write_scheduling_policy::writer_priority),
				_readyFlag(false), _cancelled(false)
			{}
			future_body(const T &value): _value(new T(value)),
			_valueMutex(boost::read_write_scheduling_policy::writer_priority),
			_readyFlag(true)
			{}
			virtual ~future_body() {}
			virtual void setValue(const T &value)
			{
				{
					boost::read_write_mutex::scoped_write_lock lock(_valueMutex);
					_readyFlag = true;
					_value.reset(new T(value));
				}
				_readyCondition.locking_notify_all();
				this->_updateSignal();
			}
			virtual bool ready() const
			{
				boost::read_write_mutex::scoped_read_lock lock(_valueMutex);
				return _readyFlag;
			}
			virtual const T& getValue() const
			{
				{
					_readyCondition.locking_wait(boost::bind(&poet::detail::future_body<T>::readyOrCancelled, this));
					if(!ready())
					{
						throw future_cancelled();
					}
				}
				boost::read_write_mutex::scoped_read_lock lock(_valueMutex);
				return *_value;
			}
			virtual void cancel()
			{
				bool emitSignal = false;
				{
					boost::mutex::scoped_lock lock(_cancelledMutex);
					if(_cancelled = false)
					{
						emitSignal = true;
						_cancelled = true;
					}
				}
				if(emitSignal) this->_updateSignal();
			}
			virtual bool cancelled() const
			{
				boost::mutex::scoped_lock lock(_cancelledMutex);
				return _cancelled;
			}
		private:
			bool readyOrCancelled() const
			{
				return ready() || cancelled();
			}

			boost::scoped_ptr<T> _value;
			mutable boost::read_write_mutex _valueMutex;
			bool _readyFlag;
			mutable typename detail::condition _readyCondition;
			bool _cancelled;
			mutable boost::mutex _cancelledMutex;
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
				_conversionFunction(conversionFunction),
				_proxyValueMutex(boost::read_write_scheduling_policy::writer_priority)
			{
				_readyConnection = _actualFutureBody->connectUpdate(boost::ref(
					detail::future_body_proxy<ProxyType, ActualType>::_updateSignal));
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
					boost::read_write_mutex::scoped_read_lock read_lock(_proxyValueMutex);
					if(_proxyValue)
					{
						initialized = true;
					}
				}// read_lock destructs here
				if(initialized == false)
				{
					{
						boost::read_write_mutex::scoped_write_lock write_lock(_proxyValueMutex);
						// make sure _proxyValue is still uninitialized after we have write lock
						if(_proxyValue == 0)
						{
							_proxyValue.reset(new ProxyType(_conversionFunction(_actualFutureBody->getValue())));
						}
					}// write_lock destructs here
				}
				boost::read_write_mutex::scoped_read_lock read_lock(_proxyValueMutex);
				return *_proxyValue;
			}
			virtual void cancel()
			{
				_actualFutureBody->cancel();
			}
			virtual bool cancelled() const
			{
				return _actualFutureBody->cancelled();
			}
		private:
			boost::shared_ptr<future_body_base<ActualType> > _actualFutureBody;
			boost::function<ProxyType (const ActualType&)> _conversionFunction;
			boost::signalslib::connection _readyConnection;
			mutable boost::scoped_ptr<ProxyType> _proxyValue;
			mutable boost::read_write_mutex _proxyValueMutex;
		};
	}

	/*! \brief A handle to a future value.

	Futures are wrappers around values which may not exist yet.  They are used to support asyncronous
	function calls.  Since a Future can be
	returned before any result is actually ready, an asyncronous call can return a Future
	immediately without blocking the calling thread.  The calling thread can then poll the
	future to determine when a result is ready, or block on the Future waiting
	for a result.

	\section default_methods Default copy constructor and assignment operator

	The default copy constructor and assignment operator produce two Futures which
	both refer to the same value.
	*/
	template <typename T> class future
	{
	public:
		template <typename OtherType> friend class future;
		/*! T */
		typedef T value_type;
		/*! boost::signal<void ()>::slot_type */
		typedef typename detail::future_body_base<T>::update_signal_type::slot_type update_slot_type;

		/*! Creates a new future with an uninitialized value. */
		future(): _futureBody(new detail::future_body<T>())
		{}
		/*! Creates a new Future with an initialized value, and provides
		implicit conversion from a value to the corresponding Future.
		*/
		future(const T &value): _futureBody(new detail::future_body<T>(value))
		{}
		/*! Creates a future from another future with a compatible template type.  The
		two Futures will both reference the same value. */
		template <typename OtherType> future(const future<OtherType> &other)
		{
			assert(typeid(T) != typeid(OtherType));
			boost::function<T (const OtherType&)> typedConversionFunction =
				boost::bind(&detail::defaultConversionFunction<T, OtherType>, _1);
			_futureBody.reset(new detail::future_body_proxy<T, OtherType>(
				other._futureBody, typedConversionFunction));
		}
		/* ConversionFunctionType is a template type instead of
		boost::function<T (const OtherType&)> because boost::bind has problems
		binding to it otherwise
		*/
		/*! Creates a future whose value is obtained
		by applying <em>conversionFunction</em> to the <em>other</em>'s value when it is
		ready.  The future is created immediately, without waiting for
		<em>other</em> to become ready.
		\param other The value of this future is based on the value referenced
		by <em>other</em>.
		\param conversionFunction A function which should be able to take
		an argument of type OtherType and return a value of type T.
		*/
		template <typename OtherType, typename ConversionFunctionType>
		future(const future<OtherType> &other,
			ConversionFunctionType conversionFunction)
		{
			boost::function<T (const OtherType&)> typedConversionFunction =
				conversionFunction;
			_futureBody.reset(new detail::future_body_proxy<T, OtherType>(
				other._futureBody, typedConversionFunction));
		}
		/*! Virtual destructor. */
		virtual ~future() {}
		/*! \returns true if the future's value is initialized. */
		bool ready() const
		{
			if(_futureBody == 0) return false;
			return _futureBody->ready();
		}
		/*! The conversion operator is used to obtain an initialized value from a future.
		If the future is not ready, the conversion
		operator will block until the future's value is initialized, or
		cancel() is called on a future that references the same value.
		\exception future_cancelled if the conversion fails due to
		cancellation.
		*/
		operator const T&() const {return _futureBody->getValue();}
		/*! Assignment from a value initializes the future's value.  This future, and any
		other which references the same value will become ready.  */
		const future<T>& operator =(const T &value)
		{
			_futureBody->setValue(value);
			return *this;
		}
		/*! Assignment from a future<U> is supported if U is implicitly convertible to T.
		The assignment happens immediately, and does not block waiting for <em>other</em> to become ready. */
		template <typename OtherType> const future<T>& operator =(const future<OtherType> &other)
		{
			assert(typeid(T) != typeid(OtherType));
			_futureBody.reset(new detail::future_body_proxy<T, OtherType>(other._futureBody));
		}
		/*! Connect a slot to be run when the future's status changes, either because
		its value has been initialized or the future has been cancelled.
		*/
		boost::signalslib::connection connectUpdate(const update_slot_type &slot)
		{
			if(_futureBody == 0) throw std::invalid_argument("Future doesn't refer to any value yet. Cannot connect slot.");
			return _futureBody->connectUpdate(slot);
		}
		/*! Cancel a future.  Any Futures doing blocking waits on the value referenced by
		this future will throw future_cancelled exceptions. */
		void cancel()
		{
			_futureBody->cancel();
		}
		/*! \returns true if this future's value has been cancelled.
		*/
		bool cancelled() const
		{
			return _futureBody->cancelled();
		}
	private:
		boost::shared_ptr<detail::future_body_base<T> > _futureBody;
	};
}

#endif // _POET_FUTURE_H
