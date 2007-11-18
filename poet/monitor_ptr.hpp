/*
	A wrapper which automatically locks/unlocks a mutex whenever the wrapped
	objects members are accessed.  See "Wrapping C++ Member Function Calls"
	by Bjarne Stroustroup at http://www.research.att.com/~bs/wrapper.pdf

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-08-27
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_HPP
#define _POET_MONITOR_HPP

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <poet/detail/monitor_base_decl.hpp>
#include <poet/detail/monitor_synchronizer.hpp>

namespace boost
{
	class mutex;
};

namespace poet
{
	template<typename T, typename Mutex> class monitor_ptr;

	template<typename T, typename Mutex>
	class monitor_call_proxy
	{
	public:
		~monitor_call_proxy()
		{}
		T* operator->() {return _pointer;}
	private:
		friend class monitor_ptr<T, Mutex>;

		monitor_call_proxy(T *pointer, boost::shared_ptr<typename Mutex::scoped_lock> lock): _pointer(pointer),
			_lock(lock)
		{};
		monitor_call_proxy(const monitor_call_proxy &rhs);	// suppress default copy constructor
		monitor_call_proxy& operator=(const monitor_call_proxy &rhs);	// suppress default assignment operator

		T *_pointer;
		boost::shared_ptr<typename Mutex::scoped_lock> _lock;
	};

	// uses default copy constructor/assignment operators
	template<typename T, typename Mutex = boost::mutex>
	class monitor_ptr
	{
	public:
		typedef T element_type;

		monitor_ptr(boost::shared_ptr<T> smart_pointer): _pointer(smart_pointer),
			_syncer(new detail::monitor_synchronizer<Mutex>())
		{
			set_monitor_ptr(_pointer.get());
		}
		monitor_ptr(T *pointer): _pointer(pointer),
			_syncer(new detail::monitor_synchronizer<Mutex>())
		{
			set_monitor_ptr(_pointer.get());
		}
		monitor_call_proxy<T, Mutex> operator->() const
		{
			boost::shared_ptr<typename Mutex::scoped_lock> shared_lock(new typename Mutex::scoped_lock(_syncer->_mutex));
			_syncer->set_current_lock(shared_lock);
			return monitor_call_proxy<T, Mutex>(_pointer.get(), shared_lock);
		}
		// unlocked access
		const boost::shared_ptr<T>& direct() const {return _pointer;}
	private:
		void set_monitor_ptr(const monitor_base *monitor)
		{
			monitor->set_synchronizer(_syncer);
		}
		void set_monitor_ptr(...)
		{}

		boost::shared_ptr<T> _pointer;
		boost::shared_ptr<detail::monitor_synchronizer<Mutex> > _syncer;
	};
};

#endif // _POET_MONITOR_HPP
