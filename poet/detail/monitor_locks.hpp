/*
	Scoped lock classes, providing implementations for locks nested in
	monitor_ptr and monitor classes.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-08-27
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_LOCKS_HPP
#define _POET_MONITOR_LOCKS_HPP

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/exceptions.hpp>
#include <poet/detail/monitor_synchronizer.hpp>

namespace poet
{
	namespace detail
	{
		template<typename T, typename Mutex, typename Lock = typename Mutex::scoped_lock>
		class monitor_scoped_lock: boost::noncopyable
		{
		public:
			monitor_scoped_lock(const boost::shared_ptr<detail::monitor_synchronizer<Mutex> > &syncer,
				const boost::shared_ptr<T> &pointer):
				_syncer(syncer),
				_lock(_syncer->_mutex, true),
				_pointer(pointer)
			{
				set_wait_function();
			}
			monitor_scoped_lock(const boost::shared_ptr<detail::monitor_synchronizer<Mutex> > &syncer,
				const boost::shared_ptr<T> &pointer, bool do_lock):
				_syncer(syncer),
				_lock(_syncer->_mutex, do_lock),
				_pointer(pointer)
			{
				if(do_lock)
					set_wait_function();
			}

			bool locked() const {return _lock.locked();}
			operator const void*() const {return static_cast<const void*>(_lock);}
			void lock()
			{
				_lock.lock();
				set_wait_function();
			}
			void unlock()
			{
				_lock.unlock();
			}
			T* operator->() const
			{
				if(locked() == false)
				{
					throw boost::lock_error();
				}
				return _pointer.get();
			}
			T& operator*() const
			{
				if(locked() == false)
				{
					throw boost::lock_error();
				}
				return *_pointer;
			}
		protected:
			void set_wait_function()
			{
				typename detail::monitor_synchronizer<Mutex>::wait_function_type wait_func = boost::bind(
					&poet::detail::monitor_scoped_lock<T, Mutex, Lock>::wait_function, this, _1, _2);
				_syncer->set_wait_function(wait_func);
			}

			boost::shared_ptr<detail::monitor_synchronizer<Mutex> > _syncer;
			Lock _lock;
			boost::shared_ptr<T> _pointer;
		private:
			friend class detail::monitor_synchronizer<Mutex>;

			void wait_function(boost::condition &condition, const boost::function<bool ()> &pred)
			{
				if(pred == 0)
					condition.wait(_lock);
				else
					condition.wait(_lock, pred);
			}
		};

		template<typename T, typename Mutex, typename Lock = typename Mutex::scoped_try_lock>
		class monitor_scoped_try_lock: public monitor_scoped_lock<T, Mutex, Lock>
		{
			typedef monitor_scoped_lock<T, Mutex, Lock> base_class;
		public:
			monitor_scoped_try_lock(const boost::shared_ptr<detail::monitor_synchronizer<Mutex> > &syncer,
				const boost::shared_ptr<T> &pointer):
				base_class(syncer, pointer, false)
			{
				try_lock();
			}
			monitor_scoped_try_lock(const boost::shared_ptr<detail::monitor_synchronizer<Mutex> > &syncer,
				const boost::shared_ptr<T> &pointer, bool do_lock):
				base_class(syncer, pointer, do_lock)
			{}
			bool try_lock()
			{
				bool locked = this->_lock.try_lock();
				if(locked)
				{
					this->set_wait_function();
				}
				return locked;
			}
		};

		template<typename T, typename Mutex, typename Lock = typename Mutex::scoped_timed_lock>
		class monitor_scoped_timed_lock: public monitor_scoped_try_lock<T, Mutex, Lock>
		{
			typedef monitor_scoped_try_lock<T, Mutex, Lock> base_class;
		public:
			template<typename Timeout>
			monitor_scoped_timed_lock(const boost::shared_ptr<detail::monitor_synchronizer<Mutex> > &syncer,
				const boost::shared_ptr<T> &pointer, const Timeout &t):
				base_class(syncer, pointer, false)
			{
				timed_lock(t);
			}
			monitor_scoped_timed_lock(const boost::shared_ptr<detail::monitor_synchronizer<Mutex> > &syncer,
				const boost::shared_ptr<T> &pointer, bool do_lock):
				base_class(syncer, pointer, do_lock)
			{}
			template<typename Timeout>
			bool timed_lock(const Timeout &t)
			{
				bool locked = this->_lock.timed_lock(t);
				if(locked)
				{
					this->set_wait_function();
				}
				return locked;
			}
		};
	};
};

#endif // _POET_MONITOR_LOCKS_HPP
