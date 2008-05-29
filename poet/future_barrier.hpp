/*
	Provides future_barrier and future_combining_barrier free factory functions,
	which create composite futures which becomes ready based on the states of
	a group of input futures.  A future returned by future_barrier becomes
	ready when all of the input futures become ready or have exceptions.
	The future_combining_barrier allows a return value to be generated
	from the values of the input futures via a user supplied "Combiner"
	function.

	begin: Frank Hess <frank.hess@nist.gov>  2008-05-20
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_FUTURE_BARRIER_HPP
#define _POET_FUTURE_BARRIER_HPP

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/type_traits.hpp>
#include <iterator>
#include <poet/detail/nonvoid.hpp>
#include <poet/future.hpp>
#include <vector>

namespace poet
{
	namespace detail
	{
		template<typename Lock>
		class scoped_unlocker
		{
		public:
			scoped_unlocker(Lock &lock): _lock(lock), _owns_lock(lock.owns_lock())
			{
				if(_owns_lock)
					_lock.unlock();
			}
			~scoped_unlocker()
			{
				if(_owns_lock && _lock.owns_lock() == false)
					_lock.lock();
			}
			void lock() {_lock.lock();}
			void unlock() {_lock.unlock();}
		private:
			Lock &_lock;
			bool _owns_lock;
		};
		template<typename Mutex>
		class unscoped_lock
		{
		public:
			unscoped_lock(Mutex &mutex): _lock(mutex, boost::adopt_lock_t())
			{}
			~unscoped_lock()
			{
				_lock.release();
			}
			void lock()
			{
				_lock.lock();
			}
			void unlock()
			{
				_lock.unlock();
			}
			bool owns_lock() const
			{
				return _lock.owns_lock();
			}
		private:
			boost::unique_lock<Mutex> _lock;
		};

		class future_barrier_body_impl: public boost::noncopyable
		{
			typedef boost::signal<void ()> update_signal_type;
		public:
			future_barrier_body_impl(boost::function<void ()> completion_callback,
			boost::function<void ()> combiner_invoker):
				_ready_count(0), _completion_callback(completion_callback), _combiner_invoker(combiner_invoker),
				_ready(false)
			{}
			template<typename InputFutureIterator>
			void set_input_futures(InputFutureIterator future_begin, InputFutureIterator future_end, const boost::shared_ptr<void> &owner)
			{
				InputFutureIterator it;
				unsigned i = 0;
				for(it = future_begin; it != future_end; ++it, ++i)
				{
					_dependency_completes.push_back(false);
					update_signal_type::slot_type update_slot(&future_barrier_body_impl::check_dependency, this, get_future_body(*it), i);
					update_slot.track(owner);
					get_future_body(*it)->connectUpdate(update_slot);
					check_dependency(get_future_body(*it), i);
				}
			}
			bool ready() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return _ready;
			}
			void join() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				_condition.wait(lock, boost::bind(&future_barrier_body_impl::nolock_complete, this));
			}
			bool timed_join(const boost::system_time &absolute_time) const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return _condition.timed_wait(lock, absolute_time, boost::bind(&future_barrier_body_impl::nolock_complete, this));
			}
			exception_ptr get_exception_ptr() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return _exception;
			}
			boost::signal<void ()> completion_signal;
		private:
			void check_dependency(const boost::shared_ptr<future_body_untyped_base > &dependency, unsigned dependency_index) const
			{
				bool complete = false;
				{
					boost::unique_lock<boost::mutex> lock(_mutex);
					if(_dependency_completes.at(dependency_index) == false)
					{
						const bool dep_ready = dependency->ready();
						const bool dep_has_exception = dependency->get_exception_ptr();
						if(dep_has_exception && _exception == false)
						{
							_dependency_completes.at(dependency_index) = true;
							_exception = dependency->get_exception_ptr();
							_condition.notify_all();
							complete = true;
						}
						if(dep_ready)
						{
							_dependency_completes.at(dependency_index) = true;
							_ready_count += dep_ready;
							if(_ready_count == _dependency_completes.size())
							{
								relocking_run_combiner(lock);
								_condition.notify_all();
								complete = true;
							}
						}
					}
				}
				if(complete)
				{
					_completion_callback();
				}
			}
			template<typename Lock>
			void relocking_run_combiner(Lock &lock) const
			{
				BOOST_ASSERT(lock.owns_lock());
				BOOST_ASSERT(!_ready && !_exception);
				exception_ptr ep;
				{
					scoped_unlocker<Lock> unlocker(lock);
					try
					{
						_combiner_invoker();
					}catch(...)
					{
						ep = current_exception();
					}
				}
				_exception = ep;
				_ready = !_exception;
			}
			bool nolock_complete() const
			{
				return _ready || _exception;
			}

			std::vector<boost::signalslib::connection> _connections;
			mutable boost::mutex _mutex;
			mutable boost::condition _condition;
			mutable std::vector<bool> _dependency_completes;
			mutable unsigned _ready_count;
			mutable boost::function<void ()> _completion_callback;
			mutable boost::function<void ()> _combiner_invoker;
			mutable bool _ready;
			mutable poet::exception_ptr _exception;
		};

		template<typename T>
		typename nonvoid<T>::type nonvoid_future_get(const future<T> &f)
		{
			return f.get();
		}
		template<>
		nonvoid<void>::type nonvoid_future_get<void>(const future<void> &f)
		{
			return nonvoid<void>::type();
		}

		template<typename R, typename Combiner, typename T>
		class combiner_invoker
		{
		public:
			combiner_invoker(const Combiner &combiner):
				_combiner(combiner)
			{}
			template<typename InputFutureIterator>
			void operator()(boost::optional<R> &result, InputFutureIterator begin, InputFutureIterator end)
			{
				std::vector<typename nonvoid<T>::type> input_values;
				std::transform(begin, end, std::back_inserter(input_values),
					boost::bind(&nonvoid_future_get<T>, _1));
				result = _combiner(input_values.begin(), input_values.end());
			}
		private:
			Combiner _combiner;
		};
		template<typename Combiner, typename T>
		class combiner_invoker<void, Combiner, T>
		{
		public:
			combiner_invoker(const Combiner &combiner):
				_combiner(combiner)
			{}
			template<typename InputFutureIterator>
			void operator()(boost::optional<nonvoid<void>::type> &result, InputFutureIterator begin, InputFutureIterator end)
			{
				std::vector<typename nonvoid<T>::type> input_values;
				std::transform(begin, end, std::back_inserter(input_values),
					boost::bind(&nonvoid_future_get<T>, _1));
				_combiner(input_values.begin(), input_values.end());
				result = null_type();
			}
		private:
			Combiner _combiner;
		};

		/* future_body for futures returned by future_barrier.  Becomes ready
			only when all the futures on its list have become ready (or have exceptions)
		*/
		template<typename R, typename Combiner, typename T>
		class future_barrier_body:
			public future_body_base<R>
		{
		public:
			template<typename InputFutureIterator>
			static boost::shared_ptr<future_barrier_body> create(const Combiner &combiner,
				InputFutureIterator future_begin, InputFutureIterator future_end)
			{
				boost::shared_ptr<future_barrier_body> new_object(
					new future_barrier_body(combiner, future_begin, future_end));
				new_object->_impl.set_input_futures(new_object->_input_futures.begin(), new_object->_input_futures.end(), new_object);
				return new_object;
			}
			virtual bool ready() const
			{
				return _impl.ready();
			}
			virtual void join() const
			{
				return _impl.join();
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				return _impl.timed_join(absolute_time);
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{}
			virtual exception_ptr get_exception_ptr() const
			{
				return _impl.get_exception_ptr();
			}
			virtual const typename nonvoid<R>::type& getValue() const
			{
				this->join();
				return *this->_combiner_result;
			}
			virtual void setValue(const typename nonvoid<R>::type &value)
			{
				BOOST_ASSERT(false);
			}
		private:
			template<typename InputFutureIterator>
			future_barrier_body(const Combiner &combiner, InputFutureIterator future_begin, InputFutureIterator future_end):
				_input_futures(future_begin, future_end), _combiner_invoker(combiner),
				_impl(boost::bind(&future_barrier_body::completion_handler, this),
					boost::bind(&future_barrier_body::invoke_combiner, this))
			{}

			void invoke_combiner()
			{
				_combiner_invoker(_combiner_result, _input_futures.begin(), _input_futures.end());
			}
			void completion_handler()
			{
				this->_updateSignal();
			}

			std::vector<future<T> > _input_futures;
			combiner_invoker<R, Combiner, T> _combiner_invoker;
			boost::optional<typename nonvoid<R>::type> _combiner_result;
			future_barrier_body_impl _impl;
		};

		class null_void_combiner
		{
		public:
			typedef void result_type;
			template<typename Iterator>
			result_type operator()(Iterator, Iterator)
			{}
		};
	} // namespace detail

	template<typename InputIterator>
	future<void> future_barrier_range(InputIterator future_begin, InputIterator future_end)
	{
		typedef detail::future_barrier_body<void, detail::null_void_combiner, void> body_type;
		future<void> result = detail::create_future<void>(body_type::create(detail::null_void_combiner(), future_begin, future_end));
		return result;
	}

	template<typename R, typename Combiner, typename InputIterator>
	future<R> future_combining_barrier_range(const Combiner &combiner, InputIterator future_begin, InputIterator future_end)
	{
		typedef typename std::iterator_traits<InputIterator>::value_type input_future_type;
		typedef typename input_future_type::value_type input_value_type;
		typedef detail::future_barrier_body<R, Combiner, input_value_type> body_type;
		future<R> result = detail::create_future<R>(body_type::create(combiner, future_begin, future_end));
		return result;
	}
}

#ifndef POET_FUTURE_BARRIER_MAX_ARGS
#define POET_FUTURE_BARRIER_MAX_ARGS 10
#endif

#define BOOST_PP_ITERATION_LIMITS (1, POET_FUTURE_BARRIER_MAX_ARGS)
#define BOOST_PP_FILENAME_1 <poet/detail/future_barrier_template.hpp>
#include BOOST_PP_ITERATE()

#endif // _POET_FUTURE_BARRIER_HPP
