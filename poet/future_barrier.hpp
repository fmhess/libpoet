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

#include <iterator>
#include <poet/future.hpp>
#include <vector>

namespace poet
{
	namespace detail
	{
		/* future_body for void futures returned by future_barrier.  Becomes ready
			only when all the futures on its list have become ready (or have exceptions)
		*/
		template<typename R, typename Combiner, typename T>
		class future_barrier_body;

		template<typename Combiner>
		class future_barrier_body<void, Combiner, void>:
			public future_body_base<void>
		{
			typedef std::vector<boost::shared_ptr<future_body_base<void> > > future_dependencies_type;
		public:
			template<typename InputFutureIterator>
			future_barrier_body(Combiner combiner, InputFutureIterator future_begin, InputFutureIterator future_end):
				_ready_count(0)
			{
				InputFutureIterator it;
				unsigned i = 0;
				for(it = future_begin; it != future_end; ++it, ++i)
				{
					_dependency_readies.push_back(false);
					update_signal_type::slot_type update_slot(&future_barrier_body::check_dependency, this, it->_future_body, i);
					_connections.push_back(it->_future_body->connectUpdate(update_slot));
					check_dependency(it->_future_body, i);
				}
			}
			virtual ~future_barrier_body()
			{
				std::vector<boost::signalslib::connection>::iterator it;
				for(it = _connections.begin(); it != _connections.end(); ++it)
				{
					it->disconnect();
				}
			}
			virtual bool ready() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return nolock_ready();
			}
			virtual void join() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				_condition.wait(lock, boost::bind(&future_barrier_body::nolock_ready, this));
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return _condition.timed_wait(lock, absolute_time, boost::bind(&future_barrier_body::nolock_ready, this));
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{}
			virtual bool has_exception() const
			{
				return false;
			}
		private:
			void check_dependency(const boost::shared_ptr<future_body_base<void> > &dependency, unsigned ready_index) const
			{
				bool emit_signal = false;
				{
					boost::unique_lock<boost::mutex> lock(_mutex);
					if(_dependency_readies.at(ready_index) == false)
					{
						if(dependency->ready() || dependency->has_exception())
						{
							_dependency_readies.at(ready_index) = true;
							++_ready_count;
							if(nolock_ready())
							{
								_condition.notify_all();
								emit_signal = true;
							}
						}
					}
				}
				if(emit_signal)
				{
					this->_updateSignal();
				}
			}
			bool nolock_ready() const
			{
				return _ready_count == _dependency_readies.size();
			}

			std::vector<boost::signalslib::connection> _connections;
			mutable boost::mutex _mutex;
			mutable boost::condition _condition;
			mutable std::vector<bool> _dependency_readies;
			mutable unsigned _ready_count;
		};

		class null_void_combiner
		{
		public:
			typedef void result_type;
			result_type operator()(...)
			{}
		};
	} // namespace detail

	template<typename InputIterator>
	future<void> future_barrier_range(InputIterator future_begin, InputIterator future_end)
	{
		future<void> result;
		result._future_body.reset(new detail::future_barrier_body<void, detail::null_void_combiner, void>(detail::null_void_combiner(), future_begin, future_end));
		return result;
	}

	template<typename R, typename Combiner, typename InputIterator>
	future<R> future_combining_barrier_range(Combiner combiner, InputIterator future_begin, InputIterator future_end)
	{
		typedef typename std::iterator_traits<InputIterator>::value_type input_future_type;
		typedef typename input_future_type::value_type input_value_type;
		future<R>result;
		result._future_body.reset(new detail::future_barrier_body<R, Combiner, input_value_type>(combiner, future_begin, future_end));
		return result;
	}
}

#ifndef POET_FUTURE_BARRIER_MAX_ARGS
#define POET_FUTURE_BARRIER_MAX_ARGS 10
#endif

#define BOOST_PP_ITERATION_LIMITS (2, POET_FUTURE_BARRIER_MAX_ARGS)
#define BOOST_PP_FILENAME_1 <poet/detail/future_barrier_template.hpp>
#include BOOST_PP_ITERATE()

#endif // _POET_FUTURE_BARRIER_HPP
