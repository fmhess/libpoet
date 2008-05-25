/*
	Provides future_barrier and future_select free functions, which allow
	creation of a future which becomes ready based on the states of
	a group of input futures.  A future returned by future_barrier becomes
	ready when all of the input futures become ready or have exceptions.
	A future returned by future_select becomes ready when any one
	of the input futures becomes ready or has an exception.

	begin: Frank Hess <frank.hess@nist.gov>  2008-05-20
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_FUTURE_WAITS_HPP
#define _POET_FUTURE_WAITS_HPP

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
		class future_barrier_body:
			public future_body_base<void>
		{
			typedef std::vector<boost::shared_ptr<future_body_base<void> > > future_dependencies_type;
		public:
			template<typename InputIterator>
			future_barrier_body(InputIterator future_begin, InputIterator future_end):
				_future_dependencies(future_begin, future_end),
				_ready(false)
			{
				future_dependencies_type::iterator it;
				for(it = _future_dependencies.begin(); it != _future_dependencies.end(); ++it)
				{
					update_signal_type::slot_type update_slot(&future_barrier_body::check_dependencies, this);
					_connections.push_back((*it)->connectUpdate(update_slot));
				}
				check_dependencies();
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
				return _ready;
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
			void check_dependencies() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				future_dependencies_type::const_iterator it;
				if(_ready == false)
				{
					for(it = _future_dependencies.begin(); it != _future_dependencies.end(); ++it)
					{
						if((*it)->ready() == false && (*it)->has_exception() == false) break;
					}
					if(it == _future_dependencies.end())
					{
						 _ready = true;
						_condition.notify_all();
					}
				}
			}
			bool nolock_ready() const
			{
				return _ready;
			}

			future_dependencies_type _future_dependencies;
			std::vector<boost::signalslib::connection> _connections;
			mutable boost::mutex _mutex;
			mutable boost::condition _condition;
			mutable bool _ready;
		};

		/* future_body for void futures returned by future_select.  Becomes ready
			when any of the futures on its list have become ready or has an exception.
		*/
		template<typename T>
			class future_select_body;
			
		template<>
		class future_select_body<void>:
			public virtual future_body_base<void>
		{
			typedef boost::shared_ptr<future_body_base<void> > future_body_dependency_type;
		public:
			typedef future_body_base<void>::update_signal_type update_signal_type;

			template<typename InputIterator>
			future_select_body(InputIterator future_begin, InputIterator future_end)
			{
				InputIterator it;
				for(it = future_begin; it != future_end; ++it)
				{
					typedef update_signal_type::slot_type update_slot_type;
					update_signal_type::slot_type update_slot(&future_select_body::check_dependency, this, it->_future_body);
					_connections.push_back(it->_future_body->connectUpdate(update_slot));
					if(check_dependency(it->_future_body)) break;
				}
			}
			virtual ~future_select_body()
			{
				nolock_disconnect_all();
			}
			virtual bool ready() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return nolock_ready();
			}
			virtual void join() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				_condition.wait(lock, boost::bind(&future_select_body::nolock_ready_or_has_exception, this));
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return _condition.timed_wait(lock, absolute_time, boost::bind(&future_select_body::nolock_ready_or_has_exception, this));
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{}
			virtual bool has_exception() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				if(!_first_complete_dependency) return false;
				return _first_complete_dependency->has_exception();
			}
		protected:
			bool nolock_ready_or_has_exception() const
			{
				return _first_complete_dependency;
			}

			mutable boost::mutex _mutex;
			mutable boost::condition _condition;
			mutable future_body_dependency_type _first_complete_dependency;
		private:
			bool check_dependency(const future_body_dependency_type &dependency) const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				if(!_first_complete_dependency)
				{
					if(dependency->ready() || dependency->has_exception())
					{
						_first_complete_dependency = dependency;
						nolock_disconnect_all();
						_condition.notify_all();
					}
				}
				return _first_complete_dependency;
			}
			bool nolock_ready() const
			{
				if(!_first_complete_dependency) return false;
				return _first_complete_dependency->ready();
			}
			void nolock_disconnect_all() const
			{
				std::vector<boost::signalslib::connection>::iterator it;
				for(it = _connections.begin(); it != _connections.end(); ++it)
				{
					it->disconnect();
				}
				_connections.clear();
			}

			mutable std::vector<boost::signalslib::connection> _connections;
		};
		
		template<typename T>
		class future_select_body: public future_select_body<void>,
			public future_body_base<T>
		{
			typedef boost::shared_ptr<future_body_base<T> > future_body_dependency_type;
		public:
			typedef future_body_base<void>::update_signal_type update_signal_type;

			template<typename InputIterator>
			future_select_body(InputIterator future_begin, InputIterator future_end):
				future_select_body<void>(future_begin, future_end)
			{}
			virtual const T& getValue() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				_condition.wait(lock, boost::bind(&future_select_body<void>::nolock_ready_or_has_exception, this));
				boost::shared_ptr<future_body_base<T> > typed_dependency =
					boost::dynamic_pointer_cast<future_body_base<T> >(_first_complete_dependency);
				return typed_dependency->getValue();
			}
			virtual void setValue(const T &value)
			{
				BOOST_ASSERT(false);
			}
		};
	} // namespace detail

	template<typename InputIterator>
	future<void> future_barrier_range(InputIterator future_begin, InputIterator future_end)
	{
		std::vector<boost::shared_ptr<detail::future_body_base<void> > > arg_bodies;
		InputIterator it;
		for(it = future_begin; it != future_end; ++it)
		{
			arg_bodies.push_back(it->_future_body);
		}
		future<void>result;
		result._future_body.reset(new detail::future_barrier_body(arg_bodies.begin(), arg_bodies.end()));
		return result;
	}

	template<typename InputIterator>
	typename std::iterator_traits<InputIterator>::value_type future_select_range(InputIterator future_begin, InputIterator future_end)
	{
		typedef typename std::iterator_traits<InputIterator>::value_type future_type;
		future_type result;
		result._future_body.reset(new detail::future_select_body<typename future_type::value_type>(future_begin, future_end));
		return result;
	}
}

#ifndef POET_FUTURE_WAITS_MAX_ARGS
#define POET_FUTURE_WAITS_MAX_ARGS 10
#endif

#define BOOST_PP_ITERATION_LIMITS (2, POET_FUTURE_WAITS_MAX_ARGS)
#define BOOST_PP_FILENAME_1 <poet/detail/future_waits_template.hpp>
#include BOOST_PP_ITERATE()

#endif // _POET_FUTURE_WAITS_HPP
