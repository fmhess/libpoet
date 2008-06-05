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

#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <iterator>
#include <poet/future.hpp>
#include <poet/detail/nonvoid.hpp>
#include <deque>
#include <list>

namespace poet
{
	template<typename R, typename Combiner, typename T1>
	future<R> future_combining_barrier(const Combiner &combiner, const future<T1> &f1);

	namespace detail
	{
		template<typename T>
		const typename nonvoid<T>::type& nonvoid_future_body_get(const future_body_base<T> &body)
		{
			return body.getValue();
		}
		const nonvoid<void>::type& nonvoid_future_body_get(const future_body_untyped_base &body)
		{
			return template_static<nonvoid<void>, nonvoid<void>::type>::object;
		}

		template<typename T>
			class future_selector_body: public boost::enable_shared_from_this<future_selector_body<T> >
		{
			typedef std::list<boost::shared_ptr<typename nonvoid_future_body_base<T>::type> > dependencies_type;
			typedef promise_body<typename nonvoid<T>::type> promise_type;
			/*FIXME: we should only hold weak_ptr to futures handed out by selected(),
			and they should hold shared_ptr to this.  When future_selector is destroyed,
			it should renege on any futures handed out by selected which have no chance
			of completing. */
			typedef std::deque<boost::shared_ptr<promise_type> > selected_promises_type;
			typedef boost::shared_ptr<typename dependencies_type::iterator> dependency_eraser_type;
			struct dependency_eraser_info
			{
				dependency_eraser_info(): iterator_valid(false)
				{}
				typename dependencies_type::iterator iterator;
				bool iterator_valid;
			};
		public:
			static boost::shared_ptr<future_selector_body> create()
			{
				boost::shared_ptr<future_selector_body> new_object(new future_selector_body);
				new_object->_waiter_callbacks.set_owner(new_object);
				return new_object;
			}
			void push(const future<T> &f)
			{
				const boost::shared_ptr<typename nonvoid_future_body_base<T>::type> body = get_future_body(f);

				boost::shared_ptr<dependency_eraser_info> eraser_info(new dependency_eraser_info);
				{
					boost::unique_lock<boost::mutex> lock(_mutex);

					eraser_info->iterator = _dependencies.insert(_dependencies.end(), body);
					eraser_info->iterator_valid = true;
				}
				typedef typename future_body_untyped_base::update_signal_type::slot_type update_slot_type;
				update_slot_type update_slot(&future_selector_body::check_dependency, this,
					boost::weak_ptr<typename nonvoid_future_body_base<T>::type>(body), eraser_info);
				update_slot.track(this->shared_from_this());
				body->connectUpdate(update_slot);
				// deal with futures which completed before we got them
				try
				{
					update_slot();
				}
				catch(const boost::expired_slot &)
				{}

				typedef waiter_event_queue::slot_type event_slot_type;
				body->waiter_callbacks().connect_slot(event_slot_type(
					&waiter_event_queue::post<event_queue::event_type>, &_waiter_callbacks, _1).track(this->shared_from_this()));

				// deal with any events already in dependency's event queue
				_waiter_callbacks.post(body->waiter_callbacks().create_poll_event());
			}
			future<T> selected() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return create_future<T>(_selected->_future_body);
			}
			void pop_selected()
			{
				boost::shared_ptr<promise_type> prom_body;

				boost::unique_lock<boost::mutex> lock(_mutex);
				if(_fulfilled_promises.empty())
				{
					lock.unlock();

					prom_body.reset(new promise_type);

					typedef waiter_event_queue::slot_type event_slot_type;
					event_slot_type event_slot(&waiter_event_queue::post<event_queue::event_type>,
						&prom_body->_future_body->waiter_callbacks(), _1);
					event_slot.track(prom_body->_future_body);
					_waiter_callbacks.connect_slot(event_slot);
					// deal with any events already in our event queue
					prom_body->_future_body->waiter_callbacks().post(_waiter_callbacks.create_poll_event());

					lock.lock();
				}else
				{
					prom_body = _fulfilled_promises.front();
					_fulfilled_promises.pop_front();
				}
				_selected = prom_body;
				_selected_promises.push_back(prom_body);
			}
		private:
			future_selector_body():
				_waiter_callbacks(_mutex, _condition)
			{
				pop_selected();
			}
			void wait_event(boost::shared_ptr<promise_type> fulfilled_promise,
				const boost::shared_ptr<typename nonvoid_future_body_base<T>::type> &dependency)
			{
				bool store_promise = false;
				if(fulfilled_promise == false)
				{
					fulfilled_promise.reset(new promise_type);
					store_promise = true;
				}

				const bool dep_ready = dependency->ready();

				if(dep_ready)
				{
					fulfilled_promise->fulfill(nonvoid_future_body_get(*dependency));
				}else
				{
					exception_ptr ep = dependency->get_exception_ptr();
					fulfilled_promise->renege(ep);
				}
				if(store_promise)
				{
					boost::unique_lock<boost::mutex> lock(_mutex);
					_fulfilled_promises.push_back(fulfilled_promise);
				}
			}
			void check_dependency(const boost::weak_ptr<typename nonvoid_future_body_base<T>::type> &weak_dependency,
				const boost::shared_ptr<dependency_eraser_info> &dependency_eraser_info)
			{
					boost::shared_ptr<typename nonvoid_future_body_base<T>::type> dependency = weak_dependency.lock();
					if(!dependency)
					{
						return;
					}

				const bool dep_ready = dependency->ready();
				exception_ptr ep;

				if(!dep_ready) ep = dependency->get_exception_ptr();

				if(dep_ready || ep)
				{
					boost::shared_ptr<promise_type> fulfilled_promise;
					{
						boost::unique_lock<boost::mutex> lock(_mutex);
						if(dependency_eraser_info->iterator_valid == false)
						{
							return;
						}
						if(_selected_promises.empty() == false)
						{
							fulfilled_promise = _selected_promises.front();
							_selected_promises.pop_front();
						}
						_dependencies.erase(dependency_eraser_info->iterator);
						dependency_eraser_info->iterator_valid = false;
					}
					_waiter_callbacks.post(boost::bind(&future_selector_body::wait_event, this, fulfilled_promise, dependency));
					throw boost::expired_slot();
				}
			}
			void do_nothing() const
			{}

			waiter_event_queue _waiter_callbacks;
			mutable boost::mutex _mutex;
			mutable boost::condition _condition;
			selected_promises_type _selected_promises;
			selected_promises_type _fulfilled_promises;
			boost::shared_ptr<promise_type> _selected;
			dependencies_type _dependencies;
		};

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
				_condition.wait(lock, boost::bind(&future_select_body::check_if_complete, this, &lock));
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return _condition.timed_wait(lock, absolute_time, boost::bind(&future_select_body::check_if_complete, this, &lock));
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{}
			virtual exception_ptr get_exception_ptr() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				if(!_first_complete_dependency) return exception_ptr();
				return _first_complete_dependency->get_exception_ptr();
			}
			virtual waiter_event_queue& waiter_callbacks() const
			{
				return _waiter_callbacks;
			}
		protected:
			future_select_body(): _waiter_callbacks(this->_mutex, this->_condition)
			{}
			template<typename InputIterator>
			static void init(const boost::shared_ptr<future_select_body> &new_object, InputIterator future_begin, InputIterator future_end)
			{
				new_object->_waiter_callbacks.set_owner(new_object);

				InputIterator it;
				for(it = future_begin; it != future_end; ++it)
				{
					typedef update_signal_type::slot_type update_slot_type;
					update_slot_type update_slot(&future_select_body::check_dependency, new_object.get(),
						get_weak_future_body(*it));
					update_slot.track(new_object);
					get_future_body(*it)->connectUpdate(update_slot);
					if(new_object->check_dependency(get_future_body(*it))) break;

					typedef typename waiter_event_queue::slot_type event_slot_type;
					get_future_body(*it)->waiter_callbacks().connect_slot(event_slot_type(
						&waiter_event_queue::post<event_queue::event_type>, &new_object->waiter_callbacks(), _1).
							track(new_object));
					// deal with any events already in dependency's event queue
					new_object->waiter_callbacks().post(get_future_body(*it)->waiter_callbacks().create_poll_event());

					new_object->_dependencies.push_back(get_future_body(*it));
				}
			}

			mutable future_body_dependency_type _first_complete_dependency;
		private:
			bool check_dependency(const boost::weak_ptr<future_body_untyped_base> &weak_dependency) const
			{
				boost::shared_ptr<future_body_untyped_base> dependency(weak_dependency);
				bool emit_signal = false;
				bool is_complete;
				{
					boost::unique_lock<boost::mutex> lock(_mutex);
					if(!_first_complete_dependency)
					{
						if(dependency->ready() || dependency->get_exception_ptr())
						{
							_first_complete_dependency = dependency;
							_dependencies.clear();
							emit_signal = true;
							_condition.notify_all();
						}
					}
					is_complete = _first_complete_dependency;
				}
				if(emit_signal)
				{
					this->_updateSignal();
				}
				return is_complete;
			}
			bool check_if_complete(boost::unique_lock<boost::mutex> *lock) const
			{
				if(_first_complete_dependency) return true;
				lock->unlock();
				_waiter_callbacks.poll();
				lock->lock();
				return _first_complete_dependency;
			}
			bool nolock_ready() const
			{
				if(!_first_complete_dependency) return false;
				return _first_complete_dependency->ready();
			}

			mutable std::vector<boost::shared_ptr<future_body_untyped_base> > _dependencies;
			mutable waiter_event_queue _waiter_callbacks;
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
				boost::shared_ptr<future_body_base<T> > typed_dependency;
				{
					boost::unique_lock<boost::mutex> lock(_mutex);
					_condition.wait(lock, boost::bind(&future_select_body<void>::check_if_complete, this, &lock));
					typed_dependency =
						boost::dynamic_pointer_cast<future_body_base<T> >(_first_complete_dependency);
				}
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

	template<typename T>
		class future_selector: public boost::noncopyable // FIXME: haven't implemented proper copy semantics yet
	{
	public:
		future_selector(): _selector_body(detail::future_selector_body<T>::create())
		{}
		future<T> selected() const
		{
			return _selector_body->selected();
		}
		void pop_selected()
		{
			_selector_body->pop_selected();
		}
		void push(const future<T> &f)
		{
			_selector_body->push(f);
		}
		template<typename Converter, typename U>
		void push(const Converter &converter, const future<U> &f)
		{
			future<T> converted_f = future_combining_barrier(converter, f);
			push(converted_f);
		}
	private:
		boost::shared_ptr<detail::future_selector_body<T> > _selector_body;
	};

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
