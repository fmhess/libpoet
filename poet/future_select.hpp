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
#include <boost/config.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <cstddef>
#include <iterator>
#include <poet/future.hpp>
#include <poet/detail/nonvoid.hpp>
#include <poet/detail/utility.hpp>
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
		inline const nonvoid<void>::type& nonvoid_future_body_get(const future_body_untyped_base &body)
		{
			return template_static<nonvoid<void>, nonvoid<void>::type>::object;
		}

		template<typename T>
			class future_selector_body: public boost::enable_shared_from_this<future_selector_body<T> >
		{
			typedef std::list<boost::shared_ptr<typename nonvoid_future_body_base<T>::type> > dependencies_type;
			typedef future_body<typename nonvoid<T>::type> dependent_type;
			typedef std::deque<boost::weak_ptr<dependent_type> > selected_container_type;
			typedef std::deque<boost::shared_ptr<dependent_type> > fulfilled_container_type;
			struct dependency_eraser_info
			{
				dependency_eraser_info(): iterator_valid(false)
				{}
				boost::signals2::connection waiter_connection;
				typename dependencies_type::iterator iterator;
				bool iterator_valid;
			};
		public:
			static boost::shared_ptr<future_selector_body> create()
			{
				boost::shared_ptr<future_selector_body> new_object(new future_selector_body);
				new_object->_waiter_callbacks.set_owner(new_object);
				new_object->pop_selected();
				return new_object;
			}
			boost::shared_ptr<future_selector_body> clone() const
			{
				boost::shared_ptr<future_selector_body> new_object(new future_selector_body(*this));
				new_object->_waiter_callbacks.set_owner(new_object);
				typename dependencies_type::iterator it;
				for(it = new_object->_dependencies.begin(); it != new_object->_dependencies.end(); ++it)
				{
					new_object->connect_to_dependency(it);
				}
				return new_object;
			}
			void push(const future<T> &f)
			{
				const boost::shared_ptr<typename nonvoid_future_body_base<T>::type> body = get_future_body(f);
				const boost::shared_ptr<dependency_eraser_info> eraser_info(new dependency_eraser_info);
				typename dependencies_type::iterator dep_it;
				{
					boost::unique_lock<boost::mutex> lock(_mutex);

					dep_it = _dependencies.insert(_dependencies.end(), body);
					++_dependencies_size;
				}
				connect_to_dependency(dep_it);
			}
			future<T> selected() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return create_future<T>(_selected);
			}
			void pop_selected()
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				if(_fulfilled_promises.empty())
				{
					lock.unlock();

					boost::shared_ptr<dependent_type> dependent = dependent_type::create();
					dependent->waiter_callbacks().observe(_waiter_callbacks);

					/* stick a shared_ptr that owns this onto the dependent so it will keep us alive
					as long as it needs us. */
					dependent->add_dependency(this->shared_from_this());

					lock.lock();
					_selected = dependent;
					_selected_promises.push_back(dependent);
				}else
				{
					_selected = _fulfilled_promises.front();
					_fulfilled_promises.pop_front();
				}
			}
			std::ptrdiff_t size() const
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				return _dependencies_size +_fulfilled_promises.size() - _selected_promises.size() + 1;
			}
			/* detach means the user isn't going to call push or pop_selected any more,
			since future_selector object has destructed.  So cancel futures that
			cannot possibly complete otherwise. */
			void detach()
			{
				boost::unique_lock<boost::mutex> lock(_mutex);
				const int extra_selected = _selected_promises.size() - _dependencies_size;
				int i;
				boost::shared_ptr<dependent_type> dependent;
				for(i = 0; i < extra_selected; ++i)
				{
					dependent = _selected_promises.back().lock();
					if(dependent) dependent->cancel(copy_exception(uncertain_future()));
					_selected_promises.pop_back();
				}

				_selected.reset();
				_fulfilled_promises.clear();
			}
		private:
			future_selector_body():
				_waiter_callbacks(_mutex, _condition), _dependencies_size(0)
			{
			}
			future_selector_body(const future_selector_body &other):
				_waiter_callbacks(_mutex, _condition)
			{
				boost::unique_lock<boost::mutex> lock(other._mutex);

				/* _selected_promises in new_object should have same size as original,
				but only has expired weak_ptr, since the original will handle fulfilling
				the its real _selected_promises. */
				_selected_promises.resize(other._selected_promises.size());
				_selected = other._selected;
				_fulfilled_promises = other._fulfilled_promises;
				_dependencies = other._dependencies;
				_dependencies_size = other._dependencies_size;
			}
			void wait_event(typename dependencies_type::iterator completed_dependency)
			{
				boost::shared_ptr<typename nonvoid_future_body_base<T>::type> dependency;
				boost::shared_ptr<dependent_type> fulfilled_promise;
 				{
					boost::unique_lock<boost::mutex> lock(_mutex);
					if(_selected_promises.empty())
					{
						fulfilled_promise = dependent_type::create();
						_fulfilled_promises.push_back(fulfilled_promise);
					}else
					{
						fulfilled_promise = _selected_promises.front().lock();
						_selected_promises.pop_front();
					}
					dependency = *completed_dependency;
					_dependencies.erase(completed_dependency);
					--_dependencies_size;
				}
				if(fulfilled_promise == false) return;

				const bool dep_ready = dependency->ready();

				if(dep_ready)
				{
					fulfilled_promise->setValue(nonvoid_future_body_get(*dependency));
				}else
				{
					exception_ptr ep = dependency->get_exception_ptr();
					fulfilled_promise->cancel(ep);
				}
			}
			void check_dependency(const boost::weak_ptr<typename nonvoid_future_body_base<T>::type> &weak_dependency,
				const boost::shared_ptr<dependency_eraser_info> &dependency_eraser_info)
			{
				boost::shared_ptr<typename nonvoid_future_body_base<T>::type> dependency = weak_dependency.lock();
				if(!dependency)
				{
					throw boost::signals2::expired_slot();
				}

				const bool dep_ready = dependency->ready();
				exception_ptr ep;

				if(!dep_ready) ep = dependency->get_exception_ptr();

				if(dep_ready || ep)
				{
					{
						boost::unique_lock<boost::mutex> lock(_mutex);
						if(dependency_eraser_info->iterator_valid == false)
						{
							throw boost::signals2::expired_slot();
						}
						dependency_eraser_info->iterator_valid = false;
						dependency_eraser_info->waiter_connection.disconnect();
					}
					_waiter_callbacks.post(boost::bind(&future_selector_body::wait_event, this, dependency_eraser_info->iterator));
					throw boost::signals2::expired_slot();
				}
			}
			void connect_to_dependency(typename dependencies_type::iterator dep_it)
			{
				const boost::shared_ptr<typename nonvoid_future_body_base<T>::type> body = *dep_it;

				boost::shared_ptr<dependency_eraser_info> eraser_info(new dependency_eraser_info);
				eraser_info->iterator = dep_it;
				eraser_info->iterator_valid = true;
				eraser_info->waiter_connection = _waiter_callbacks.observe(body->waiter_callbacks());

				typedef typename future_body_untyped_base::update_signal_type::slot_type update_slot_type;
				update_slot_type update_slot(&future_selector_body::check_dependency, this,
					make_weak(body), eraser_info);
				update_slot.track(this->shared_from_this());
				boost::signals2::connection conn;
				conn = body->connectUpdate(update_slot);
				// deal with futures which completed before we got them
				try
				{
					update_slot();
				}
				catch(const boost::signals2::expired_slot &)
				{
					conn.disconnect();
				}
			}

			waiter_event_queue _waiter_callbacks;
			mutable boost::mutex _mutex;
			mutable boost::condition _condition;
			selected_container_type _selected_promises;
			fulfilled_container_type _fulfilled_promises;
			boost::shared_ptr<dependent_type> _selected;
			dependencies_type _dependencies;
			std::size_t _dependencies_size;
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
			// needed by msvc9
			template<typename U>
			friend class future_select_body;
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
				boost::unique_lock<boost::mutex> lock(mutex());
				return nolock_ready();
			}
			virtual void join() const
			{
				boost::unique_lock<boost::mutex> lock(mutex());
				condition().wait(lock, boost::bind(&future_select_body::check_if_complete, this, &lock));
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				boost::unique_lock<boost::mutex> lock(mutex());
				return condition().timed_wait(lock, absolute_time, boost::bind(&future_select_body::check_if_complete, this, &lock));
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{}
			virtual exception_ptr get_exception_ptr() const
			{
				boost::unique_lock<boost::mutex> lock(mutex());
				if(!_first_complete_dependency) return exception_ptr();
				return _first_complete_dependency->get_exception_ptr();
			}
			virtual waiter_event_queue& waiter_callbacks() const
			{
				return _waiter_callbacks;
			}
		protected:
			future_select_body(): _waiter_callbacks(future_body_untyped_base::mutex(), future_body_untyped_base::condition())
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
						make_weak(get_future_body(*it)));
					update_slot.track(new_object);
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

					new_object->waiter_callbacks().observe(get_future_body(*it)->waiter_callbacks());

					new_object->_dependencies.push_back(get_future_body(*it));
				}
			}
			bool check_if_complete(boost::unique_lock<boost::mutex> *lock) const
			{
				if(_first_complete_dependency) return true;
				lock->unlock();
				_waiter_callbacks.poll();
				lock->lock();
				return _first_complete_dependency;
			}

			mutable future_body_dependency_type _first_complete_dependency;
		private:
			void check_dependency(const boost::weak_ptr<future_body_untyped_base> &weak_dependency) const
			{
				boost::shared_ptr<future_body_untyped_base> dependency(weak_dependency);
				if(dependency == false)
				{
					throw boost::signals2::expired_slot();
				}
				bool emit_signal = false;
				{
					boost::unique_lock<boost::mutex> lock(mutex());
					if(!_first_complete_dependency)
					{
						if(dependency->ready() || dependency->get_exception_ptr())
						{
							_first_complete_dependency = dependency;
							_dependencies.clear();
							emit_signal = true;
							condition().notify_all();
						}
					}
				}
				if(emit_signal)
				{
					this->_updateSignal();
					_waiter_callbacks.close_posting();
					throw boost::signals2::expired_slot();
				}
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
					boost::unique_lock<boost::mutex> lock(mutex());
					condition().wait(lock, boost::bind(&future_select_body<void>::check_if_complete, this, &lock));
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
#ifdef BOOST_MSVC
#pragma warning( suppress : 4250 ) // suppress msvc 9 warning
#endif
		};
	} // namespace detail

	template<typename T>
		class future_selector
	{
	public:
		typedef future<T> value_type;
		typedef std::size_t size_type;
		typedef std::ptrdiff_t difference_type;

		future_selector(): _selector_body(detail::future_selector_body<T>::create())
		{}
		future_selector(const future_selector &other):
			_selector_body(other._selector_body->clone())
		{}

		~future_selector()
		{
			_selector_body->detach();
		}

		future_selector& operator=(const future_selector &rhs)
		{
			if(&rhs == this) return *this;
			future_selector temp(rhs);
			swap(temp);
			return *this;
		}
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
		template<typename Converter, typename ExceptionHandler, typename U>
		void push(const Converter &converter, const ExceptionHandler &exception_handler, const future<U> &f)
		{
			future<T> converted_f = future_combining_barrier<T>(converter, exception_handler, f);
			push(converted_f);
		}
		difference_type size() const
		{
			return _selector_body->size();
		}
		void reset()
		{
			future_selector temp;
			swap(temp);
		}
		void swap(future_selector &other)
		{
			using std::swap;
			swap(_selector_body, other._selector_body);
		}
	private:
		boost::shared_ptr<detail::future_selector_body<T> > _selector_body;
	};

	template<typename T>
	void swap(future_selector<T> &a, future_selector<T> &b)
	{
		a.swap(b);
	}

	template<typename InputIterator>
	typename std::iterator_traits<InputIterator>::value_type future_select_range(InputIterator future_begin, InputIterator future_end)
	{
		typedef typename std::iterator_traits<InputIterator>::value_type future_type;
		if(future_begin == future_end) return future_type();
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
