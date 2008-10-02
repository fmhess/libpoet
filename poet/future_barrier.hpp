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

#include <boost/config.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/type_traits.hpp>
#include <iterator>
#include <poet/detail/identity.hpp>
#include <poet/detail/nonvoid.hpp>
#include <poet/detail/template_static.hpp>
#include <poet/detail/utility.hpp>
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

		class future_barrier_body_base: public virtual future_body_untyped_base
		{
		public:
			virtual bool ready() const
			{
				boost::unique_lock<boost::mutex> lock(mutex());
				return _ready;
			}
			virtual void join() const
			{
				boost::unique_lock<boost::mutex> lock(mutex());
				condition().wait(lock, boost::bind(&future_barrier_body_base::check_if_complete, this, &lock));
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				boost::unique_lock<boost::mutex> lock(mutex());
				return condition().timed_wait(lock, absolute_time, boost::bind(&future_barrier_body_base::check_if_complete, this, &lock));
			}
			virtual void cancel(const poet::exception_ptr&)
			{}
			virtual exception_ptr get_exception_ptr() const
			{
				boost::unique_lock<boost::mutex> lock(mutex());
				return _exception;
			}
			virtual waiter_event_queue& waiter_callbacks() const
			{
				return _waiter_callbacks;
			}
		protected:
			typedef boost::function<exception_ptr (const exception_ptr &)> exception_handler_type;

			future_barrier_body_base(const boost::function<void ()> &bound_combiner,
				const exception_handler_type &exception_handler):
				_waiter_callbacks(future_body_untyped_base::mutex(), future_body_untyped_base::condition()),
				_ready_count(0), _ready(false), _bound_combiner(bound_combiner),
				_exception_handler(exception_handler), _combiner_event_posted(false)
			{}

			template<typename U, typename FutureIterator>
			static void init(const boost::shared_ptr<U> &owner, FutureIterator future_begin, FutureIterator future_end)
			{
				owner->_waiter_callbacks.set_owner(owner);

				// handle special case of no dependencies
				if(future_begin == future_end)
				{
					owner->_combiner_event_posted = true;
					owner->_waiter_callbacks.post(boost::bind(&future_barrier_body_base::waiter_event, owner.get(), exception_ptr()));
					owner->_waiter_callbacks.close_posting();
					return;
				}

				FutureIterator it;
				unsigned i = 0;
				for(it = future_begin; it != future_end; ++it, ++i)
				{
					owner->_dependency_completes.push_back(false);
					update_signal_type::slot_type update_slot(&future_barrier_body_base::check_dependency,
						owner.get(), make_weak(get_future_body(*it)), i);
					update_slot.track(owner);
					boost::signals2::connection conn;
					conn = get_future_body(*it)->connectUpdate(update_slot);
					try
					{
						update_slot();
					}
					catch(const boost::signals2::expired_slot &)
					{
						conn.disconnect();
						break;
					}

					owner->waiter_callbacks().observe(get_future_body(*it)->waiter_callbacks());
				}
			}
		private:
			void waiter_event(const exception_ptr &dependency_exception)
			{
				exception_ptr ep;
				if(dependency_exception)
				{
					try
					{
						ep = _exception_handler(dependency_exception);
					}catch(...)
					{
						BOOST_ASSERT(false);
					}
				}else
				{
					try
					{
						_bound_combiner();
					}catch(...)
					{
						ep = current_exception();
					}
				}
				{
					boost::unique_lock<boost::mutex> lock(this->mutex());
					_exception = ep;
					_ready = !_exception;
					condition().notify_all();
				}
				this->_updateSignal();
			}
			void check_dependency(const boost::weak_ptr<future_body_untyped_base > &weak_dependency,
				unsigned dependency_index)
			{
				boost::shared_ptr<future_body_untyped_base> dependency(weak_dependency);
				exception_ptr dependency_exception;
				bool all_ready = false;
				bool received_first_exception = false;
				{
					boost::unique_lock<boost::mutex> lock(mutex());

					if(_combiner_event_posted) return;

					if(_dependency_completes.at(dependency_index) == false)
					{
						const bool dep_ready = dependency->ready();
						dependency_exception = dependency->get_exception_ptr();
						if(dependency_exception && _exception == false)
						{
							_dependency_completes.at(dependency_index) = true;
							received_first_exception = true;
							_combiner_event_posted = true;
						}
						if(dep_ready)
						{
							_dependency_completes.at(dependency_index) = true;
							_ready_count += dep_ready;
							if(_ready_count == _dependency_completes.size())
							{
								all_ready = true;
								_combiner_event_posted = true;
							}
						}
					}
				}
				if(all_ready || received_first_exception)
				{
					_waiter_callbacks.post(boost::bind(&future_barrier_body_base::waiter_event, this, dependency_exception));
					_waiter_callbacks.close_posting();
					throw boost::signals2::expired_slot();
				}
			}
			bool check_if_complete(boost::unique_lock<boost::mutex> *lock) const
			{
				// do initial check to make sure we don't run any wait callbacks if we are already complete
				const bool complete = _ready || _exception;
				if(complete) return complete;

				lock->unlock();
				_waiter_callbacks.poll();
				lock->lock();
				return _ready || _exception;
			}

			mutable waiter_event_queue _waiter_callbacks;
			mutable std::vector<bool> _dependency_completes;
			mutable unsigned _ready_count;
			mutable bool _ready;
			mutable boost::function<void ()> _bound_combiner;
			mutable exception_handler_type _exception_handler;
			mutable bool _combiner_event_posted;
		};

		template<typename T>
		const typename nonvoid<T>::type& nonvoid_future_get(const future<T> &f)
		{
			return f.get();
		}
		template<>
			inline const nonvoid<void>::type& nonvoid_future_get<void>(const future<void> &f)
		{
			return template_static<nonvoid<void>, nonvoid<void>::type>::object;
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
					&nonvoid_future_get<T>);
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
					&nonvoid_future_get<T>);
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
			public future_body_base<R>, public future_barrier_body_base
		{
			typedef future_barrier_body_base barrier_base_class;
		public:
			template<typename InputFutureIterator>
			static boost::shared_ptr<future_barrier_body> create(const Combiner &combiner,
				const barrier_base_class::exception_handler_type &exception_handler,
				InputFutureIterator future_begin, InputFutureIterator future_end)
			{
				boost::shared_ptr<future_barrier_body> new_object(new future_barrier_body(combiner, exception_handler,
					future_begin, future_end));
				barrier_base_class::init(new_object, new_object->_input_futures.begin(), new_object->_input_futures.end());
				return new_object;
			}
			using barrier_base_class::ready;
			using barrier_base_class::join;
			using barrier_base_class::timed_join;
			using barrier_base_class::cancel;
			using barrier_base_class::get_exception_ptr;
			virtual const typename nonvoid<R>::type& getValue() const
			{
				this->join();
				if(this->_exception)
				{
					rethrow_exception(this->_exception);
				}
				return *this->_combiner_result;
			}
			virtual void setValue(const typename nonvoid<R>::type &value)
			{
				BOOST_ASSERT(false);
			}
		private:
			template<typename InputFutureIterator>
			future_barrier_body(const Combiner &combiner,
				const barrier_base_class::exception_handler_type &exception_handler,
				InputFutureIterator future_begin, InputFutureIterator future_end):
#ifdef BOOST_MSVC
#pragma warning( suppress : 4355 ) // suppress msvc 9 warning
#endif
				barrier_base_class(boost::bind(&future_barrier_body::invoke_combiner, this), exception_handler),
				_input_futures(future_begin, future_end),
				_combiner_invoker(combiner)
			{}

			void invoke_combiner()
			{
				boost::optional<typename nonvoid<R>::type> result;
				_combiner_invoker(result, _input_futures.begin(), _input_futures.end());
				{
					boost::unique_lock<boost::mutex> lock(mutex());
					_combiner_result = result;
				}
			}

			std::vector<future<T> > _input_futures;
			combiner_invoker<R, Combiner, T> _combiner_invoker;
			boost::optional<typename nonvoid<R>::type> _combiner_result;
#ifdef BOOST_MSVC
#pragma warning( suppress : 4250 ) // suppress msvc 9 warning
#endif
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
		future<void> result = detail::create_future<void>(body_type::create(detail::null_void_combiner(), detail::identity(),
			future_begin, future_end));
		return result;
	}

	template<typename R, typename Combiner, typename ExceptionHandler, typename InputIterator>
	future<R> future_combining_barrier_range(const Combiner &combiner, const ExceptionHandler &exception_handler,
		InputIterator future_begin, InputIterator future_end)
	{
		typedef typename std::iterator_traits<InputIterator>::value_type input_future_type;
		typedef typename input_future_type::value_type input_value_type;
		typedef detail::future_barrier_body<R, Combiner, input_value_type> body_type;
		future<R> result = detail::create_future<R>(body_type::create(combiner, exception_handler, future_begin, future_end));
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
