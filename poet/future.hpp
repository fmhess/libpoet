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
#include <boost/thread/thread_time.hpp>
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
		typedef int bogus_future_void_type;

		template <typename T> class future_body_base
		{
		public:
			typedef typename boost::signal<void ()> update_signal_type;

			future_body_base()
			{}
			virtual ~future_body_base() {}
			virtual bool ready() const = 0;
			virtual const T& getValue() const = 0;
			virtual bool timed_join(const boost::system_time &absolute_time) const = 0;
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
					boost::unique_lock<boost::mutex> lock(_mutex);
					if(_exception == 0 && !_value)
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
					boost::unique_lock<boost::mutex> lock(_mutex);
				return _value;
			}
			virtual const T& getValue() const
			{
				_readyCondition.locking_wait(boost::bind(&poet::detail::future_body<T>::readyOrCancelled, this));
				boost::unique_lock<boost::mutex> lock(_mutex);
				if(_exception)
				{
					rethrow_exception(_exception);
				}
				BOOST_ASSERT(_value);
				return _value.get();
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				_readyCondition.locking_timed_wait(absolute_time, boost::bind(&poet::detail::future_body<T>::readyOrCancelled, this));
				return readyOrCancelled();
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{
				bool emitSignal = false;
				{
					boost::unique_lock<boost::mutex> lock(_mutex);
					if(_exception == 0 && !_value)
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
				boost::unique_lock<boost::mutex> lock(_mutex);
				return _exception;
			}
		private:
			bool readyOrCancelled() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return _value || _exception;
			}

			boost::optional<T> _value;
			mutable boost::mutex _mutex;
			mutable typename detail::condition _readyCondition;
			poet::exception_ptr _exception;
		};

		template<typename ProxyType, typename ActualType>
		static ProxyType default_conversion_function(const ActualType& actualValue)
		{
			return ProxyType(actualValue);
		}
		template<typename ActualType>
		static int null_conversion_function(const ActualType& actualValue)
		{
			return 0;
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
				message << __FUNCTION__ << ": didn't ever expect this function to actually get called!";
				throw std::invalid_argument(message.str());
			}
			virtual bool ready() const
			{
				return _actualFutureBody->ready();
			}
			virtual const ProxyType& getValue() const
			{
				bool initialized = false;
				{
					boost::unique_lock<boost::mutex> lock(_mutex);
					if(_proxyValue)
					{
						initialized = true;
					}
				}// read_lock destructs here
				if(initialized == false)
				{
					{
						boost::unique_lock<boost::mutex> lock(_mutex);
						// make sure _proxyValue is still uninitialized after we have write lock
						if(_proxyValue == false)
						{
							_proxyValue = _conversionFunction(_actualFutureBody->getValue());
						}
					}// write_lock destructs here
				}
				boost::unique_lock<boost::mutex> lock(_mutex);
				return _proxyValue.get();
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				return _actualFutureBody->timed_join(absolute_time);
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
			typedef typename future<T>::update_slot_type slot_type;
			future_value.connect_update(slot_type(&detail::promise_body<T>::handle_future_fulfillment, _pimpl, future_value));
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
	private:
		boost::shared_ptr<detail::promise_body<T> > _pimpl;
	};

	// void specialization
	template<>
	class promise<void>: private promise<detail::bogus_future_void_type>
	{
	private:
		typedef promise<detail::bogus_future_void_type> base_type;
	public:
		template <typename U>
		friend class future;

		typedef void value_type;

		promise()
		{}
		promise(const promise<void> &other): promise<detail::bogus_future_void_type>(other)
		{}
		// allow conversion from a promise with any template type to a promise<void>
		template <typename OtherType>
		promise(const promise<OtherType> &other)
		{
			boost::function<int (const OtherType&)> conversion_function =
				boost::bind(&detail::null_conversion_function<OtherType>, _1);
			_pimpl->_future_body.reset(new detail::future_body_proxy<detail::bogus_future_void_type, OtherType>(
				other._pimpl->_future_body, conversion_function));
		}
		virtual ~promise() {}
		void fulfill()
		{
			base_type::fulfill(0);
		}
		inline void fulfill(const future<void> &future_value);
		template <typename E>
		void renege(const E &exception)
		{
			base_type::renege(exception);
		}
		void renege(const poet::exception_ptr &exp)
		{
			base_type::renege(exp);
		}
	};

	template <typename T> class future
	{
	public:
		template <typename OtherType> friend class future;
		friend class future<void>;

		typedef T value_type;
		typedef typename detail::future_body_base<T>::update_signal_type::slot_type update_slot_type;

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
			_future_body.reset(new detail::future_body_proxy<T, OtherType>(
				other._future_body, typedConversionFunction));
		}
		future()
		{}
		virtual ~future() {}
		bool ready() const
		{
			if(_future_body == 0) return false;
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
		bool timed_join(const boost::system_time &absolute_time) const
		{
			return _future_body->timed_join(absolute_time);
		}
		template <typename OtherType> const future<T>& operator=(const future<OtherType> &other)
		{
			BOOST_ASSERT(typeid(T) != typeid(OtherType));
			_future_body.reset(new detail::future_body_proxy<T, OtherType>(other._future_body));
			return *this;
		}
		boost::signalslib::connection connect_update(const update_slot_type &slot) const
		{
			if(_future_body == 0) throw std::invalid_argument("Future doesn't refer to any value yet. Cannot connect slot.");
			return _future_body->connectUpdate(slot);
		}
		void cancel()
		{
			_future_body->cancel(poet::copy_exception(cancelled_future()));
		}
		bool has_exception() const
		{
			if(_future_body == 0) return true;
			return _future_body->has_exception();
		}
	private:
		boost::shared_ptr<detail::future_body_base<T> > _future_body;
	};

	template <>
	class future<void>: private future<detail::bogus_future_void_type>
	{
	private:
		typedef future<detail::bogus_future_void_type> base_type;
	public:
		template <typename OtherType> friend class future;
		friend class promise<void>;

		typedef void value_type;
		typedef base_type::update_slot_type update_slot_type;

		future(const promise<void> &promise_in): base_type(reinterpret_cast<const promise<detail::bogus_future_void_type> &>(promise_in))
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
			boost::function<int (const OtherType&)> typedConversionFunction =
				boost::bind(&detail::null_conversion_function<OtherType>, _1);
			_future_body.reset(new detail::future_body_proxy<detail::bogus_future_void_type, OtherType>(
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
		template <typename OtherType> const future<void>& operator=(const future<OtherType> &other)
		{
			BOOST_ASSERT(typeid(void) != typeid(OtherType));
			_future_body.reset(new detail::future_body_proxy<detail::bogus_future_void_type, OtherType>(other._future_body));
			return *this;
		}
		using base_type::timed_join;
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
	future_value.connect_update(slot_type(&detail::promise_body<detail::bogus_future_void_type>::handle_future_fulfillment, _pimpl,
		reinterpret_cast<const future<detail::bogus_future_void_type> &>(future_value)));
}

#endif // _POET_FUTURE_H
