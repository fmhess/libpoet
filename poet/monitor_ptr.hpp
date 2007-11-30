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

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <poet/detail/monitor_locks.hpp>
#include <poet/detail/monitor_synchronizer.hpp>
#include <poet/monitor_base.hpp>

namespace poet
{
	// uses default copy constructor/assignment operators
	template<typename T, typename Mutex = boost::mutex>
	class monitor_ptr
	{
	public:
		typedef T element_type;
		typedef Mutex mutex_type;
		typedef detail::monitor_scoped_lock<T, Mutex> scoped_lock;

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
		template<typename U>
		explicit monitor_ptr(U *pointer): _pointer(pointer),
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
		operator bool() const {return _pointer;}
	private:
		template<typename U, typename M, typename L>
		friend class detail::monitor_scoped_lock;
		template<typename U, typename M, typename L>
		friend class detail::monitor_scoped_try_lock;
		template<typename U, typename M, typename L>
		friend class detail::monitor_scoped_timed_lock;

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
		typedef detail::monitor_scoped_try_lock<T, Mutex> scoped_try_lock;

		try_monitor_ptr()
		{}
		try_monitor_ptr(boost::shared_ptr<T> smart_pointer): base_class(smart_pointer)
		{}
		template<typename U>
		explicit try_monitor_ptr(U *pointer): base_class(pointer)
		{}
	};

	template<typename T, typename Mutex = boost::timed_mutex>
	class timed_monitor_ptr: public try_monitor_ptr<T, Mutex>
	{
		typedef try_monitor_ptr<T, Mutex> base_class;
	public:
		typedef detail::monitor_scoped_timed_lock<T, Mutex> scoped_timed_lock;

		timed_monitor_ptr()
		{}
		timed_monitor_ptr(boost::shared_ptr<T> smart_pointer): base_class(smart_pointer)
		{}
		template<typename U>
		explicit timed_monitor_ptr(U *pointer): base_class(pointer)
		{}
	};
};

#endif // _POET_MONITOR_PTR_HPP
