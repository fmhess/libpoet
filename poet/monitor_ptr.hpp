/*
	A wrapper which automatically locks/unlocks a mutex whenever the wrapped
	objects members are accessed.  See "Wrapping C++ Member Function Calls"
	by Bjarne Stroustroup at http://www.research.att.com/~bs/wrapper.pdf

	begin: Frank Mori Hess <fmhess@users.sourceforge.net>  2007-08-27
	copyright (c) Frank Mori Hess 2007-2008
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_PTR_HPP
#define _POET_MONITOR_PTR_HPP

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <poet/monitor_locks.hpp>
#include <poet/detail/monitor_synchronizer.hpp>
#include <poet/monitor_base.hpp>
#include <poet/monitor_locks.hpp>

namespace poet
{
	template<typename Monitor>
	class monitor_unique_lock;
	namespace detail
	{
		template<typename Monitor, typename MonitorHandle, typename Lock>
		class lock_wrapper;
	}

	// uses default copy constructor/assignment operators
	template<typename T, typename Mutex = boost::mutex>
	class monitor_ptr
	{
	public:
		typedef T element_type;
		typedef Mutex mutex_type;

		class scoped_lock: public monitor_unique_lock<const monitor_ptr>
		{
			typedef monitor_unique_lock<const monitor_ptr> base_class;
		public:
			scoped_lock(monitor_ptr &monitor_pointer):
				base_class(monitor_pointer)
			{}
			explicit scoped_lock(const monitor_ptr<T, Mutex> &monitor_pointer):
				base_class(monitor_pointer)
			{}
			scoped_lock(monitor_ptr &monitor_pointer, bool do_lock):
				base_class(monitor_pointer, boost::defer_lock_t())
			{
				if(do_lock) base_class::lock();
			}
			explicit scoped_lock(const monitor_ptr &monitor_pointer, bool do_lock):
				base_class(monitor_pointer, boost::defer_lock_t())
			{
				if(do_lock) base_class::lock();
			}
		};

		class scoped_try_lock: public monitor_unique_lock<const monitor_ptr>
		{
			typedef monitor_unique_lock<const monitor_ptr> base_class;
		public:
			scoped_try_lock(monitor_ptr &monitor_pointer):
				base_class(monitor_pointer, boost::try_to_lock_t())
			{}
			explicit scoped_try_lock(const monitor_ptr &monitor_pointer):
				base_class(monitor_pointer, boost::try_to_lock_t())
			{}
			scoped_try_lock(monitor_ptr &monitor_pointer, bool do_lock):
				base_class(monitor_pointer, boost::defer_lock_t())
			{
				if(do_lock) base_class::lock();
			}
			explicit scoped_try_lock(const monitor_ptr &monitor_pointer, bool do_lock):
				base_class(monitor_pointer, boost::defer_lock_t())
			{
				if(do_lock) base_class::lock();
			}
		};

		class scoped_timed_lock: public monitor_unique_lock<const monitor_ptr>
		{
			typedef monitor_unique_lock<const monitor_ptr> base_class;
		public:
			template<typename Timeout>
			scoped_timed_lock(monitor_ptr &monitor_pointer, const Timeout &timeout):
				base_class(monitor_pointer, timeout)
			{}
			template<typename Timeout>
			explicit scoped_timed_lock(const monitor_ptr<T, Mutex> &monitor_pointer, const Timeout &timeout):
				base_class(monitor_pointer, timeout)
			{}
			scoped_timed_lock(monitor_ptr<T, Mutex> &monitor_pointer, bool do_lock):
				base_class(monitor_pointer, boost::defer_lock_t())
			{
				if(do_lock) base_class::lock();
			}
			explicit scoped_timed_lock(const monitor_ptr<T, Mutex> &monitor_pointer, bool do_lock):
				base_class(monitor_pointer, boost::defer_lock_t())
			{
				if(do_lock) base_class::lock();
			}
		};

		monitor_ptr()
		{}
		monitor_ptr(const boost::shared_ptr<T> &smart_pointer): _pointer(smart_pointer),
			_syncer(new detail::monitor_synchronizer<Mutex>())
		{
			set_monitor_ptr(_pointer.get());
		}
		template<typename U>
		explicit monitor_ptr(U *pointer): _pointer(pointer),
			_syncer(new detail::monitor_synchronizer<Mutex>())
		{
			set_monitor_ptr(_pointer.get());
		}
		// support implicit conversions
		template<typename U>
		monitor_ptr(const monitor_ptr<U, Mutex> &other): _pointer(other._pointer),
			_syncer(other._syncer)
		{}
		// aliasing constructor
		template<typename U>
		monitor_ptr(const monitor_ptr<U, Mutex> &other, T *ptr):
			_pointer(other._pointer, ptr), _syncer(other._syncer)
		{}

		virtual ~monitor_ptr() {}

		monitor_unique_lock<const monitor_ptr> operator->() const
		{
			return monitor_unique_lock<const monitor_ptr>(*this);
		}
		// unlocked access
		const boost::shared_ptr<T>& direct() const {return _pointer;}

		void reset(const boost::shared_ptr<T> &smart_pointer)
		{
			_pointer = smart_pointer;
			_syncer.reset(new detail::monitor_synchronizer<Mutex>());
			set_monitor_ptr(_pointer.get());
		};
		template<typename U>
		void reset(U *pointer)
		{
			boost::shared_ptr<T> smart_pointer(pointer);
			reset(smart_pointer);
		};
		void reset()
		{
			_pointer.reset();
			_syncer.reset();
		}
		// aliasing reset
		template<typename U>
		void reset(const monitor_ptr<U, Mutex> &other, T *raw_ptr)
		{
			_pointer.reset(other._pointer, raw_ptr);
			_syncer = other._syncer;
			set_monitor_ptr(_pointer.get());
		};

		operator bool() const {return _pointer;}

		void swap(monitor_ptr &other)
		{
			using std::swap;
			swap(_pointer, other._pointer);
			swap(_syncer, other._syncer);
		}

		// Boost.Threads interface for Lockable concepts
		// Lockable
		void lock() const
		{
			get_syncer()->_mutex.lock();
		}
		bool try_lock() const
		{
			return get_syncer()->_mutex.try_lock();
		}
		void unlock() const
		{
			get_syncer()->_mutex.unlock();
		}
		// TimedLockable
		template<typename Timeout>
		bool timed_lock(const Timeout &timeout) const
		{
			return get_syncer()->_mutex.timed_lock(timeout);
		}
		// SharedLockable
		void lock_shared() const
		{
			get_syncer()->_mutex.lock_shared();
		}
		bool try_lock_shared() const
		{
			return get_syncer()->_mutex.try_lock_shared();
		}
		template<typename Timeout>
		bool timed_lock_shared(const Timeout &timeout) const
		{
			return get_syncer()->_mutex.timed_lock_shared(timeout);
		}
		void unlock_shared() const
		{
			get_syncer()->_mutex.unlock_shared();
		}
		void unlock_and_lock_shared() const
		{
			get_syncer()->_mutex.unlock_and_lock_shared();
		}
		// UpgradeLockable
		void lock_upgrade() const
		{
			get_syncer()->_mutex.lock_upgrade();
		}
		void unlock_upgrade() const
		{
			get_syncer()->_mutex.unlock_upgrade();
		}
		void unlock_upgrade_and_lock() const
		{
			get_syncer()->_mutex.unlock_upgrade_and_lock();
		}
		void unlock_upgrade_and_lock_shared() const
		{
			get_syncer()->_mutex.unlock_upgrade_and_lock_shared();
		}
		void unlock_and_lock_upgrade() const
		{
			get_syncer()->_mutex.unlock_and_lock_upgrade();
		}
	private:
		template<typename U, typename M>
		friend class monitor_ptr;
		template<typename Monitor, typename MonitorHandle, typename Lock>
		friend class detail::lock_wrapper;

		void set_monitor_ptr(const monitor_base *monitor)
		{
			monitor->set_synchronizer(_syncer);
		}
		void set_monitor_ptr(...)
		{}

		const boost::shared_ptr<detail::monitor_synchronizer<Mutex> > & get_syncer() const
		{
			if(!_syncer) throw boost::lock_error();
			return _syncer;
		}

		boost::shared_ptr<T> _pointer;
		boost::shared_ptr<detail::monitor_synchronizer<Mutex> > _syncer;
	};

	template<typename T, typename MutexA, typename MutexB>
	inline bool operator==(const monitor_ptr<T, MutexA> &ptr0, const monitor_ptr<T, MutexB> &ptr1)
	{
		return ptr0.direct() == ptr1.direct();
	}

	template<typename T, typename MutexA, typename MutexB>
	inline bool operator!=(const monitor_ptr<T, MutexA> &ptr0, const monitor_ptr<T, MutexB> &ptr1)
	{
		return ptr0.direct() != ptr1.direct();
	}

	template<typename T, typename MutexA, typename MutexB>
	inline bool operator<(const monitor_ptr<T, MutexA> &ptr0, const monitor_ptr<T, MutexB> &ptr1)
	{
		return ptr0.direct() < ptr1.direct();
	}

	template<typename T, typename U, typename Mutex>
	inline monitor_ptr<T, Mutex> static_pointer_cast(const monitor_ptr<U, Mutex> &pointer)
	{
		return monitor_ptr<T, Mutex>(pointer, static_cast<T*>(pointer.direct().get()));
	}
	template<typename T, typename U, typename Mutex>
	inline monitor_ptr<T, Mutex> dynamic_pointer_cast(const monitor_ptr<U, Mutex> &pointer)
	{
		return monitor_ptr<T, Mutex>(pointer, dynamic_cast<T*>(pointer.direct().get()));
	}
	template<typename T, typename U, typename Mutex>
	inline monitor_ptr<T, Mutex> const_pointer_cast(const monitor_ptr<U, Mutex> &pointer)
	{
		return monitor_ptr<T, Mutex>(pointer, const_cast<T*>(pointer.direct().get()));
	}

	template<typename T, typename Mutex>
	void swap(poet::monitor_ptr<T, Mutex> &mon0, poet::monitor_ptr<T, Mutex> &mon1)
	{
		mon0.swap(mon1);
	}

	template<typename T, typename Mutex>
	monitor_ptr<T, Mutex> & get_monitor_ptr(monitor_ptr<T, Mutex> &mon)
	{
		return mon;
	}
	template<typename T, typename Mutex>
	const monitor_ptr<T, Mutex> & get_monitor_ptr(const monitor_ptr<T, Mutex> &mon)
	{
		return mon;
	}
};

#endif // _POET_MONITOR_PTR_HPP
