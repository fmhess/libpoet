/*
	Extensions of Boost.Thread lock classes, which add support for
	operator->() and operator*().  Used in conjunction with
	monitor_ptr and monitor classes.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-08-27
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_LOCKS_HPP
#define _POET_MONITOR_LOCKS_HPP

#include <boost/bind.hpp>
#include <boost/config.hpp>
#include <boost/function.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/exceptions.hpp>
#include <boost/thread/locks.hpp>
#include <poet/detail/monitor_synchronizer.hpp>
#include <poet/monitor_ptr.hpp>

namespace poet
{
	template<typename T, typename Mutex>
	class monitor_ptr;
	template<typename T, typename Mutex>
	class monitor;
	template<typename Monitor>
	class monitor_upgrade_lock;
	template<typename UpgradeLock>
	class monitor_upgrade_to_unique_lock;
	template<typename T, typename U, typename Mutex>
	inline monitor_ptr<T, Mutex> const_pointer_cast(const monitor_ptr<U, Mutex> &pointer);

	namespace detail
	{
		// figure out const-correct monitor_ptr type to use as handle
		// default
		template<typename Monitor>
		class monitor_handle
		{
		public:
			typedef monitor_ptr<typename Monitor::element_type, typename Monitor::mutex_type> type;
		};
		// partial specialization for poet::monitor, whose constness is transitive
		template<typename T, typename Mutex>
		class monitor_handle<const monitor<T, Mutex> >
		{
		public:
			typedef monitor_ptr<const T, Mutex> type;
		};

		template<typename Monitor, typename MonitorHandle, typename Lock>
		class lock_wrapper
		{
			typedef MonitorHandle monitor_ptr_type;
		public:
			typedef Monitor monitor_type;
			typedef typename monitor_ptr_type::element_type element_type;

			lock_wrapper(): _mon_raw_ptr(0)
			{}
			explicit lock_wrapper(Monitor &mon):
				_mon(get_monitor_ptr(mon)), _mon_raw_ptr(&mon),
				_lock(_mon.get_syncer()->_mutex)
			{
				set_wait_function();
			}
			template<typename U>
			lock_wrapper(Monitor &mon, const U &arg):
				_mon(get_monitor_ptr(mon)), _mon_raw_ptr(&mon),
				_lock(_mon.get_syncer()->_mutex, arg)
			{
				set_wait_function();
			}
			template<typename OtherLock>
			lock_wrapper(boost::detail::thread_move_t<OtherLock> other):
				_mon(other->_mon), _mon_raw_ptr(other->_mon_raw_ptr), _lock(other->_lock.move())
			{
				set_wait_function();
			}

			// unique/shared_lock interface
			void swap(lock_wrapper &other)
			{
				using std::swap;
				swap(_mon, other._mon);
				swap(_mon_raw_ptr, other._mon_raw_ptr);
				_lock.swap(other._lock);
				set_wait_function();
				other.set_wait_function();
			}
			void lock()
			{
				_lock.lock();
				set_wait_function();
			}
			bool try_lock()
			{
				bool successful = _lock.try_lock();
				set_wait_function();
				return successful;
			}
			template<typename Timeout>
			bool timed_lock(const Timeout &timeout)
			{
				bool successful = _lock.timed_lock(timeout);
				set_wait_function();
				return successful;
			}
			void unlock()
			{
				_lock.unlock();
			}
			bool owns_lock() const
			{
				return _lock.owns_lock();
			}
			bool locked() const // backwards compatibility
			{
				return owns_lock();
			}
			// safe bool idiom, somewhat safer than providing conversion to bool operator
			typedef Lock * Lock::* unspecified_bool_type;
			operator unspecified_bool_type() const
			{
				return !_lock ? 0 : &_lock;
			}
			bool operator!() const
			{
				return !_lock;
			}
			Monitor* mutex() const
			{
				return _mon_raw_ptr;
			}
			Monitor* release()
			{
				Monitor * result = _mon_raw_ptr;
				_mon_raw_ptr = 0;
				_mon.reset();
				_lock.release();
				return result;
			}

			// monitor extensions to lock interface
			element_type* operator->() const
			{
				if(owns_lock() == false)
				{
					throw boost::lock_error();
				}
				return _mon.direct().get();
			}
			element_type& operator*() const
			{
				if(owns_lock() == false)
				{
					throw boost::lock_error();
				}
				return *_mon.direct().get();
			}
#ifndef BOOST_NO_MEMBER_TEMPLATE_FRIENDS
		protected:
#endif	// BOOST_NO_MEMBER_TEMPLATE_FRIENDS
			monitor_ptr_type _mon;
#ifndef BOOST_NO_MEMBER_TEMPLATE_FRIENDS
		private:
			template<typename Lockable>
			friend class monitor_shared_lock;
			template<typename Lockable>
			friend class monitor_upgrade_lock;
			template<typename UpgradeLock>
			friend class monitor_upgrade_to_unique_lock;
			template<typename M, typename MH, typename L>
			friend class lock_wrapper;
#endif	// BOOST_NO_MEMBER_TEMPLATE_FRIENDS

			void set_wait_function()
			{
				if(_lock.owns_lock())
				{
					typename detail::monitor_synchronizer<typename Monitor::mutex_type>::wait_function_type wait_func = boost::bind(
						&lock_wrapper::wait_function, this, _1, _2);
					_mon.get_syncer()->set_wait_function(wait_func);
				}
			}
			void wait_function(boost::condition &condition, const boost::function<bool ()> &pred)
			{
				if(pred == 0)
					condition.wait(_lock);
				else
					condition.wait(_lock, pred);
			}
			// non-copyable
			lock_wrapper(lock_wrapper &);
			lock_wrapper& operator=(lock_wrapper&);

			Monitor *_mon_raw_ptr;
			Lock _lock;
		};
	}

	template<typename Monitor>
	class monitor_unique_lock: public detail::lock_wrapper<Monitor,
		typename detail::monitor_handle<Monitor>::type,
		boost::unique_lock<typename Monitor::mutex_type> >
	{
		typedef detail::lock_wrapper<Monitor,
			typename detail::monitor_handle<Monitor>::type,
			boost::unique_lock<typename Monitor::mutex_type> >
			base_class;
	public:
		monitor_unique_lock()
		{}
		explicit monitor_unique_lock(Monitor &mon): base_class(mon)
		{}
		template<typename U>
		monitor_unique_lock(Monitor &mon, const U &arg):
			base_class(mon, arg)
		{}
		// move constructors
		monitor_unique_lock(boost::detail::thread_move_t<monitor_unique_lock> other):
			base_class(other)
		{}
		monitor_unique_lock(boost::detail::thread_move_t<monitor_upgrade_lock<Monitor> > other):
			base_class(other)
		{}

		// move emulation
		boost::detail::thread_move_t<monitor_unique_lock> move()
		{
			return boost::detail::thread_move_t<monitor_unique_lock>(*this);
		}
		template<typename Lock>
		monitor_unique_lock& operator=(boost::detail::thread_move_t<Lock> other)
		{
			monitor_unique_lock temp(other);
			this->swap(temp);
			return *this;
		}
		operator boost::detail::thread_move_t<monitor_unique_lock>()
		{
			return move();
		}
	};

	template<typename Monitor>
	void swap(monitor_unique_lock<Monitor> &lockA, monitor_unique_lock<Monitor> &lockB)
	{
		lockA.swap(lockB);
	}

	template<typename Monitor>
	boost::detail::thread_move_t<monitor_unique_lock<Monitor> >
		move(monitor_unique_lock<Monitor> &lock)
	{
		return lock.move();
	}

	template<typename Monitor>
	class monitor_shared_lock: public detail::lock_wrapper<Monitor,
		monitor_ptr<typename Monitor::element_type, typename Monitor::mutex_type>,
		boost::shared_lock<typename Monitor::mutex_type > >
	{
		typedef detail::lock_wrapper<Monitor,
			monitor_ptr<typename Monitor::element_type, typename Monitor::mutex_type>,
			boost::shared_lock<typename Monitor::mutex_type> >
			base_class;
	public:
		monitor_shared_lock()
		{}
		explicit monitor_shared_lock(Monitor &mon): base_class(mon)
		{}
		template<typename U>
		monitor_shared_lock(Monitor &mon, const U &arg):
			base_class(mon, arg)
		{}
		// move constructors
		monitor_shared_lock(boost::detail::thread_move_t<monitor_shared_lock> other):
			base_class(other)
		{}
		monitor_shared_lock(boost::detail::thread_move_t<monitor_upgrade_lock<Monitor> > other):
			base_class(other)
		{}
		monitor_shared_lock(boost::detail::thread_move_t<monitor_unique_lock<Monitor> > other):
			base_class(other)
		{}

		// monitor extensions to lock interface, only allow pointer to const access for shared_lock
		const typename base_class::element_type* operator->() const
		{
			if(this->owns_lock() == false)
			{
				throw boost::lock_error();
			}
			return this->_mon.direct().get();
		}
		const typename base_class::element_type& operator*() const
		{
			if(this->owns_lock() == false)
			{
				throw boost::lock_error();
			}
			return *this->_mon.direct().get();
		}

		// move emulation
		boost::detail::thread_move_t<monitor_shared_lock> move()
		{
			return boost::detail::thread_move_t<monitor_shared_lock>(*this);
		}
		template<typename Lock>
		monitor_shared_lock& operator=(boost::detail::thread_move_t<Lock> other)
		{
			monitor_shared_lock temp(other);
			this->swap(temp);
			return *this;
		}
		operator boost::detail::thread_move_t<monitor_shared_lock>()
		{
			return move();
		}
	};

	template<typename Monitor>
	void swap(monitor_shared_lock<Monitor> &lockA, monitor_shared_lock<Monitor> &lockB)
	{
		lockA.swap(lockB);
	}

	template<typename Monitor>
	boost::detail::thread_move_t<monitor_shared_lock<Monitor> >
		move(monitor_shared_lock<Monitor> &lock)
	{
		return lock.move();
	}

	template<typename Monitor>
	class monitor_upgrade_lock: public detail::lock_wrapper<Monitor,
		monitor_ptr<typename Monitor::element_type, typename Monitor::mutex_type>,
		boost::upgrade_lock<typename Monitor::mutex_type> >
	{
		typedef detail::lock_wrapper<Monitor,
			monitor_ptr<typename Monitor::element_type, typename Monitor::mutex_type>,
			boost::upgrade_lock<typename Monitor::mutex_type> >
			base_class;
	public:
		monitor_upgrade_lock()
		{}
		explicit monitor_upgrade_lock(Monitor &mon): base_class(mon)
		{}
		template<typename U>
		monitor_upgrade_lock(Monitor &mon, const U &arg):
			base_class(mon, arg)
		{}
		// move constructors
		monitor_upgrade_lock(boost::detail::thread_move_t<monitor_upgrade_lock> other):
			base_class(other)
		{}
		monitor_upgrade_lock(boost::detail::thread_move_t<monitor_unique_lock<Monitor> > other):
			base_class(other)
		{}

		// monitor extensions to lock interface, only allow pointer to const access for upgrade_lock
		const typename base_class::element_type* operator->() const
		{
			if(this->owns_lock() == false)
			{
				throw boost::lock_error();
			}
			return this->_mon.direct().get();
		}
		const typename base_class::element_type& operator*() const
		{
			if(this->owns_lock() == false)
			{
				throw boost::lock_error();
			}
			return *this->_mon.direct().get();
		}

		// move emulation
		boost::detail::thread_move_t<monitor_upgrade_lock> move()
		{
			return boost::detail::thread_move_t<monitor_upgrade_lock>(*this);
		}
		template<typename Lock>
		monitor_upgrade_lock& operator=(boost::detail::thread_move_t<Lock> other)
		{
			monitor_upgrade_lock temp(other);
			this->swap(temp);
			return *this;
		}
		operator boost::detail::thread_move_t<monitor_upgrade_lock>()
		{
			return move();
		}
	};

	template<typename Monitor>
	void swap(monitor_upgrade_lock<Monitor> &lockA, monitor_upgrade_lock<Monitor> &lockB)
	{
		lockA.swap(lockB);
	}

	template<typename Monitor>
	boost::detail::thread_move_t<monitor_upgrade_lock<Monitor> >
		move(monitor_upgrade_lock<Monitor> &lock)
	{
		return lock.move();
	}

	template<typename Monitor>
	class monitor_upgrade_to_unique_lock
	{
		typedef boost::upgrade_to_unique_lock<typename Monitor::mutex_type> wrapped_lock_type;
		typedef typename detail::monitor_handle<Monitor>::type monitor_ptr_type;
	public:
		typedef Monitor monitor_type;
		typedef typename monitor_ptr_type::element_type element_type;

		explicit monitor_upgrade_to_unique_lock(monitor_upgrade_lock<Monitor> &upgrade_lock):
			_mon(upgrade_lock._mon),
			_lock(upgrade_lock._lock)
		{}
		// move constructor
		monitor_upgrade_to_unique_lock(boost::detail::thread_move_t<monitor_upgrade_to_unique_lock> other):
			_mon(other->_mon), _lock(boost::detail::thread_move_t<wrapped_lock_type>(other->_lock))
		{}

		void swap(monitor_upgrade_to_unique_lock &other)
		{
			using std::swap;
			swap(_mon, other._mon);
			_lock.swap(other._lock);
		}
		bool operator!()
		{
			return !_lock;
		}
		// safe bool idiom, somewhat safer than providing conversion to bool operator
		typedef wrapped_lock_type * wrapped_lock_type::* unspecified_bool_type;
		operator unspecified_bool_type() const
		{
			return !_lock ? 0 : &_lock;
		}
		bool owns_lock() const
		{
			return _lock.owns_lock();
		}

		// monitor extensions to lock interface
		element_type* operator->() const
		{
			if(this->owns_lock() == false)
			{
				throw boost::lock_error();
			}
			return _mon.direct().get();
		}
		element_type& operator*() const
		{
			if(this->owns_lock() == false)
			{
				throw boost::lock_error();
			}
			return *_mon.direct().get();
		}

		// move emulation
		boost::detail::thread_move_t<monitor_upgrade_to_unique_lock> move()
		{
			return boost::detail::thread_move_t<monitor_upgrade_to_unique_lock>(*this);
		}
		monitor_upgrade_to_unique_lock& operator=(boost::detail::thread_move_t<monitor_upgrade_to_unique_lock> other)
		{
			monitor_upgrade_to_unique_lock temp(other);
			swap(temp);
			return *this;
		}
		operator boost::detail::thread_move_t<monitor_upgrade_to_unique_lock>()
		{
			return move();
		}
	private:
		monitor_ptr_type _mon;
		wrapped_lock_type _lock;
	};

	template<typename Monitor>
	void swap(monitor_upgrade_to_unique_lock<Monitor> &lockA, monitor_upgrade_to_unique_lock<Monitor> &lockB)
	{
		lockA.swap(lockB);
	}

	template<typename Monitor>
	boost::detail::thread_move_t<monitor_upgrade_to_unique_lock<Monitor> >
		move(monitor_upgrade_to_unique_lock<Monitor> &lock)
	{
		return lock.move();
	}
};

namespace boost
{
	template<typename T>
	detail::thread_move_t<poet::monitor_unique_lock<T> >
		move(const detail::thread_move_t<poet::monitor_unique_lock<T> > &x)
	{
		return x;
	}
	template<typename T>
	detail::thread_move_t<poet::monitor_shared_lock<T> >
		move(const detail::thread_move_t<poet::monitor_shared_lock<T> > &x)
	{
		return x;
	}
	template<typename T>
	detail::thread_move_t<poet::monitor_upgrade_lock<T> >
		move(const detail::thread_move_t<poet::monitor_upgrade_lock<T> > &x)
	{
		return x;
	}
	template<typename T>
	detail::thread_move_t<poet::monitor_upgrade_to_unique_lock<T> >
		move(const detail::thread_move_t<poet::monitor_upgrade_to_unique_lock<T> > &x)
	{
		return x;
	}
}

#endif // _POET_MONITOR_LOCKS_HPP
