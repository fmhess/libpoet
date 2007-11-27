/*
	A wrapper which automatically locks/unlocks a mutex whenever the wrapped
	objects members are accessed.  See "Wrapping C++ Member Function Calls"
	by Bjarne Stroustroup at http://www.research.att.com/~bs/wrapper.pdf

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-08-27
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_PTR_HPP
#define _POET_MONITOR_PTR_HPP

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/exceptions.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/weak_ptr.hpp>
#include <poet/monitor_base.hpp>
#include <poet/detail/monitor_synchronizer.hpp>

namespace poet
{
	namespace detail
	{
		template<typename T, typename Mutex, typename Lock = typename Mutex::scoped_lock>
		class monitor_ptr_scoped_lock: boost::noncopyable
		{
		public:
			monitor_ptr_scoped_lock(monitor_ptr<T, Mutex> &monitor_pointer):
				_pointer(monitor_pointer._pointer.get()),
				_syncer(monitor_pointer._syncer),
				_lock(_syncer->_mutex)
			{
				set_wait_function();
			}
			monitor_ptr_scoped_lock(monitor_ptr<T, Mutex> &monitor_pointer, bool do_lock):
				_pointer(monitor_pointer._pointer.get()),
				_syncer(monitor_pointer._syncer),
				_lock(_syncer->_mutex)
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
				return _pointer;
			}
			T& operator*() const
			{
				if(locked() == false)
				{
					throw boost::lock_error();
				}
				return *_pointer;
			}
			void wait_function(boost::condition &condition, const boost::function<bool ()> &pred)
			{
				if(pred == 0)
					condition.wait(_lock);
				else
					condition.wait(_lock, pred);
			}
		protected:
			void set_wait_function()
			{
				typename detail::monitor_synchronizer<Mutex>::wait_function_type wait_func = boost::bind(
					&poet::detail::monitor_ptr_scoped_lock<T, Mutex, Lock>::wait_function, this, _1, _2);
				_syncer->set_wait_function(wait_func);
				}

			T* _pointer;
			boost::shared_ptr<detail::monitor_synchronizer<Mutex> > _syncer;
			Lock _lock;
		};

		template<typename T, typename Mutex, typename Lock = typename Mutex::scoped_try_lock>
		class monitor_ptr_scoped_try_lock: public monitor_ptr_scoped_lock<T, Mutex, Lock>
		{
			typedef monitor_ptr_scoped_lock<T, Mutex, Lock> base_class;
		public:
			monitor_ptr_scoped_try_lock(monitor_ptr<T, Mutex> &monitor_pointer):
				base_class(monitor_pointer, false)
			{
				try_lock();
			}
			monitor_ptr_scoped_try_lock(monitor_ptr<T, Mutex> &monitor_pointer, bool do_lock):
				base_class(monitor_pointer, do_lock)
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
		class monitor_ptr_scoped_timed_lock: public monitor_ptr_scoped_try_lock<T, Mutex, Lock>
		{
			typedef monitor_ptr_scoped_try_lock<T, Mutex, Lock> base_class;
		public:
			template<typename Timeout>
			monitor_ptr_scoped_timed_lock(monitor_ptr<T, Mutex> &monitor_pointer, const Timeout &t):
				base_class(monitor_pointer, false)
			{
				timed_lock(t);
			}
			monitor_ptr_scoped_timed_lock(monitor_ptr<T, Mutex> &monitor_pointer, bool do_lock):
				base_class(monitor_pointer, do_lock)
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

	// uses default copy constructor/assignment operators
	template<typename T, typename Mutex = boost::mutex>
	class monitor_ptr
	{
	public:
		typedef T element_type;
		typedef Mutex mutex_type;
		typedef detail::monitor_ptr_scoped_lock<T, Mutex> scoped_lock;

		class call_proxy
		{
		public:
			const scoped_lock& operator->() {return *_lock;}
		private:
			friend class monitor_ptr;

			call_proxy(const boost::shared_ptr<scoped_lock> &lock):
				_lock(lock)
			{}

			boost::shared_ptr<scoped_lock> _lock;
		};

		monitor_ptr()
		{}
		monitor_ptr(boost::shared_ptr<T> smart_pointer): _pointer(smart_pointer),
			_syncer(new detail::monitor_synchronizer<Mutex>())
		{
			set_monitor_ptr(_pointer.get());
		}
		explicit monitor_ptr(T *pointer): _pointer(pointer),
			_syncer(new detail::monitor_synchronizer<Mutex>())
		{
			set_monitor_ptr(_pointer.get());
		}
		virtual ~monitor_ptr() {}

		call_proxy operator->()
		{
			return call_proxy(boost::shared_ptr<scoped_lock>(new scoped_lock(*this)));
		}
		// unlocked access
		const boost::shared_ptr<T>& direct() const {return _pointer;}
		//: TODO implement reset()
	private:
		template<typename U, typename M, typename L>
		friend class detail::monitor_ptr_scoped_lock;
		template<typename U, typename M, typename L>
		friend class detail::monitor_ptr_scoped_try_lock;
		template<typename U, typename M, typename L>
		friend class detail::monitor_ptr_scoped_timed_lock;

		void set_monitor_ptr(const monitor_base *monitor)
		{
			monitor->set_synchronizer(_syncer);
		}
		void set_monitor_ptr(...)
		{}

		boost::shared_ptr<T> _pointer;
		boost::shared_ptr<detail::monitor_synchronizer<Mutex> > _syncer;
	};

	template<typename T, typename Mutex = boost::try_mutex>
	class try_monitor_ptr: public monitor_ptr<T, Mutex>
	{
		typedef monitor_ptr<T, Mutex> base_class;
	public:
		typedef detail::monitor_ptr_scoped_try_lock<T, Mutex> scoped_try_lock;

		try_monitor_ptr()
		{}
		try_monitor_ptr(boost::shared_ptr<T> smart_pointer): base_class(smart_pointer)
		{}
		explicit try_monitor_ptr(T *pointer): base_class(pointer)
		{}
	};

	template<typename T, typename Mutex = boost::timed_mutex>
	class timed_monitor_ptr: public try_monitor_ptr<T, Mutex>
	{
		typedef try_monitor_ptr<T, Mutex> base_class;
	public:
		typedef detail::monitor_ptr_scoped_timed_lock<T, Mutex> scoped_timed_lock;

		timed_monitor_ptr()
		{}
		timed_monitor_ptr(boost::shared_ptr<T> smart_pointer): base_class(smart_pointer)
		{}
		explicit timed_monitor_ptr(T *pointer): base_class(pointer)
		{}
	};
};

#endif // _POET_MONITOR_PTR_HPP
