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
		class future_body_untyped_base
		{
		public:
			typedef boost::signal<void ()> update_signal_type;

			virtual ~future_body_untyped_base() {}
			virtual bool ready() const = 0;
			virtual void join() const = 0;
			virtual bool timed_join(const boost::system_time &absolute_time) const = 0;
			virtual void cancel(const poet::exception_ptr &) = 0;
			virtual exception_ptr get_exception_ptr() const = 0;
			boost::signalslib::connection connectUpdate(const update_signal_type::slot_type &slot)
			{
				return _updateSignal.connect(slot);
			}
		protected:
			update_signal_type _updateSignal;
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
					if(_exception == false && !_value)
					{
						_value = value;
						_readyCondition.notify_all();
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
					boost::unique_lock<boost::mutex> lock(_mutex);
				return _value;
			}
			virtual const T& getValue() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				_readyCondition.wait(lock, boost::bind(&poet::detail::future_body<T>::readyOrCancelled, this));
				if(_exception)
				{
					rethrow_exception(_exception);
				}
				BOOST_ASSERT(_value);
				return _value.get();
			}
			virtual void join() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				_readyCondition.wait(lock, boost::bind(&poet::detail::future_body<T>::readyOrCancelled, this));
				if(_exception)
				{
					rethrow_exception(_exception);
				}
				BOOST_ASSERT(_value);
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				_readyCondition.timed_wait(lock, absolute_time, boost::bind(&poet::detail::future_body<T>::readyOrCancelled, this));
				return readyOrCancelled();
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{
				bool emitSignal = false;
				{
					boost::unique_lock<boost::mutex> lock(_mutex);
					if(_exception == false && !_value)
					{
						emitSignal = true;
						_readyCondition.notify_all();
						_exception = exp;
					}
				}
				if(emitSignal)
				{
					this->_updateSignal();
				}
			}
			virtual exception_ptr get_exception_ptr() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return _exception;
			}
		private:
			bool readyOrCancelled() const
			{
				return _value || _exception;
			}

			boost::optional<T> _value;
			mutable boost::mutex _mutex;
			mutable boost::condition _readyCondition;
			poet::exception_ptr _exception;
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
				boost::unique_lock<boost::mutex> lock(_mutex);
				if(!_proxyValue)
				{
					_proxyValue = _conversionFunction(_actualFutureBody->getValue());
				}
				return _proxyValue.get();
			}
			virtual void join() const
			{
				_actualFutureBody->join();
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				return _actualFutureBody->timed_join(absolute_time);
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{
				_actualFutureBody->cancel(exp);
			}
			virtual exception_ptr get_exception_ptr() const
			{
				return _actualFutureBody->get_exception_ptr();
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
			inline void handle_future_void_fulfillment(const future<void> &future_value);

			boost::shared_ptr<future_body_base<T> > _future_body;
		};

		template<typename T>
			future<T> create_future(const boost::shared_ptr<future_body_untyped_base> &body);
		template<>
			future<void> create_future<void>(const boost::shared_ptr<future_body_untyped_base> &body);
		template<typename T>
			const boost::shared_ptr<future_body_base<T> >& get_future_body(const poet::future<T> &f);
		const boost::shared_ptr<future_body_untyped_base>& get_future_body(const poet::future<void> &f);
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
			_pimpl->_future_body.reset(new detail::future_body_proxy<detail::nonvoid<void>::type, OtherType>(
				other._pimpl->_future_body, conversion_function));
		}
		virtual ~promise() {}
		void fulfill()
		{
			base_type::fulfill(null_type());
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
		friend future<T> detail::create_future<T>(const boost::shared_ptr<detail::future_body_untyped_base> &body);
		friend const boost::shared_ptr<detail::future_body_base<T> >& detail::get_future_body<T>(const poet::future<T> &f);
	public:
		template <typename OtherType> friend class future;
		friend class future<void>;

		typedef T value_type;
		typedef typename detail::future_body_untyped_base::update_signal_type::slot_type update_slot_type;

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
		friend const boost::shared_ptr<detail::future_body_untyped_base>& detail::get_future_body(const poet::future<void> &f);
	public:
		template <typename OtherType> friend class future;
		friend class promise<void>;

		typedef detail::future_body_untyped_base::update_signal_type::slot_type update_slot_type;
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
		bool timed_join(const boost::system_time &absolute_time) const
		{
			return _future_body->timed_join(absolute_time);
		}
		bool ready() const
		{
			if(_future_body == 0) return false;
			return _future_body->ready();
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
			const boost::shared_ptr<future_body_base<T> >& get_future_body(const poet::future<T> &f)
		{
			return f._future_body;
		}
			const boost::shared_ptr<future_body_untyped_base>& get_future_body(const poet::future<void> &f)
		{
			return f._future_body;
		}
	} // namespace detail

	void promise<void>::fulfill(const future<void> &future_value)
	{
		typedef future<void>::update_slot_type slot_type;
		future_value.connect_update(slot_type(&detail::promise_body<detail::nonvoid<void>::type>::handle_future_void_fulfillment,
			_pimpl, future_value));
	}
}


#endif // _POET_FUTURE_H
