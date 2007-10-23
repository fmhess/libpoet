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

#include <boost/shared_ptr.hpp>

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
		{
// 			std::cerr << __PRETTY_FUNCTION__ << std::endl;
		}
		T* operator->() {return _pointer;}
	private:
		friend class monitor_ptr<T, Mutex>;

		monitor_call_proxy(T *pointer, Mutex *mutex): _pointer(pointer),
			_lock(*mutex)
		{
// 			std::cerr << __PRETTY_FUNCTION__ << std::endl;
		};
		monitor_call_proxy(const monitor_call_proxy &rhs);	// suppress default copy constructor
		monitor_call_proxy& operator=(const monitor_call_proxy &rhs);	// suppress default assignment operator

		T *_pointer;
		typename Mutex::scoped_lock _lock;
	};

	// uses default copy constructor/assignment operators
	template<typename T, typename Mutex = boost::mutex>
	class monitor_ptr
	{
	public:
		typedef T element_type;

		monitor_ptr(boost::shared_ptr<T> smart_pointer): _pointer(smart_pointer), _mutex(new Mutex)
		{}
		monitor_ptr(T *pointer): _pointer(pointer), _mutex(new Mutex)
		{}
		monitor_call_proxy<T, Mutex> operator->() const {return monitor_call_proxy<T, Mutex>(_pointer.get(), _mutex.get());}
		// unlocked access
		const boost::shared_ptr<T>& direct() const {return _pointer;}
	private:
		boost::shared_ptr<T> _pointer;
		boost::shared_ptr<Mutex> _mutex;
	};
};

#endif // _POET_MONITOR_HPP
