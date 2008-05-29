/*
	Provides future_select free functions, which allow
	creation of a future which becomes ready based on the states of
	a group of input futures.  A future returned by future_select
	becomes ready when any one
	of the input futures becomes ready or has an exception.

	begin: Frank Hess <frank.hess@nist.gov>  2008-05-20
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_FUTURE_SELECT_HPP
#define _POET_FUTURE_SELECT_HPP

#include <iterator>
#include <poet/future.hpp>
#include <vector>

namespace poet
{
	namespace detail
	{
		/* future_body for void futures returned by future_select.  Becomes ready
			when any of the futures on its list have become ready or has an exception.
		*/
		template<typename T>
		class future_select_body;

		template<>
		class future_select_body<void>:
			public virtual future_body_untyped_base
		{
			typedef boost::shared_ptr<future_body_untyped_base > future_body_dependency_type;
		public:
			typedef future_body_untyped_base::update_signal_type update_signal_type;

			template<typename InputIterator>
			static boost::shared_ptr<future_select_body> create(InputIterator future_begin, InputIterator future_end)
			{
				boost::shared_ptr<future_select_body> new_object(new future_select_body);
				init(new_object, future_begin, future_end);
				return new_object;
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
			virtual exception_ptr get_exception_ptr() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				if(!_first_complete_dependency) return exception_ptr();
				return _first_complete_dependency->get_exception_ptr();
			}
		protected:
			template<typename InputIterator>
			static void init(const boost::shared_ptr<future_select_body> &new_object, InputIterator future_begin, InputIterator future_end)
			{
				InputIterator it;
				for(it = future_begin; it != future_end; ++it)
				{
					typedef update_signal_type::slot_type update_slot_type;
					update_slot_type update_slot(&future_select_body::check_dependency, new_object.get(), get_future_body(*it));
					update_slot.track(new_object);
					get_future_body(*it)->connectUpdate(update_slot);
					if(new_object->check_dependency(get_future_body(*it))) break;
				}
			}
			bool nolock_ready_or_has_exception() const
			{
				return _first_complete_dependency;
			}

			mutable boost::mutex _mutex;
			mutable boost::condition _condition;
			mutable future_body_dependency_type _first_complete_dependency;
		protected:
			future_select_body()
			{}
		private:
			bool check_dependency(const future_body_dependency_type &dependency) const
			{
				bool emit_signal = false;
				{
					boost::unique_lock<boost::mutex> lock(_mutex);
					if(!_first_complete_dependency)
					{
						if(dependency->ready() || dependency->get_exception_ptr())
						{
							_first_complete_dependency = dependency;
							emit_signal = true;
							_condition.notify_all();
						}
					}
				}
				if(emit_signal)
				{
					this->_updateSignal();
				}
				return _first_complete_dependency;
			}
			bool nolock_ready() const
			{
				if(!_first_complete_dependency) return false;
				return _first_complete_dependency->ready();
			}
		};

		template<typename T>
		class future_select_body: public future_select_body<void>,
			public future_body_base<T>
		{
			typedef boost::shared_ptr<future_body_base<T> > future_body_dependency_type;
		public:
			typedef future_body_untyped_base::update_signal_type update_signal_type;

			template<typename InputIterator>
			static boost::shared_ptr<future_select_body> create(InputIterator future_begin, InputIterator future_end)
			{
				boost::shared_ptr<future_select_body> new_object(new future_select_body);
				new_object->init(new_object, future_begin, future_end);
				return new_object;
			}

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
  private:
			future_select_body()
			{}
		};
	} // namespace detail

	template<typename InputIterator>
	typename std::iterator_traits<InputIterator>::value_type future_select_range(InputIterator future_begin, InputIterator future_end)
	{
		typedef typename std::iterator_traits<InputIterator>::value_type future_type;
		typedef detail::future_select_body<typename future_type::value_type> body_type;
		future_type result = detail::create_future<typename future_type::value_type>(
			body_type::create(future_begin, future_end));
		return result;
	}
}

#ifndef POET_FUTURE_SELECT_MAX_ARGS
#define POET_FUTURE_SELECT_MAX_ARGS 10
#endif

#define BOOST_PP_ITERATION_LIMITS (2, POET_FUTURE_SELECT_MAX_ARGS)
#define BOOST_PP_FILENAME_1 <poet/detail/future_select_template.hpp>
#include BOOST_PP_ITERATE()

#endif // _POET_FUTURE_WAITS_HPP
