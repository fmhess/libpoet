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
#include <poet/detail/monitor_locks.hpp>
#include <poet/detail/monitor_synchronizer.hpp>
#include <poet/monitor_base.hpp>
#include <poet/mutex_properties.hpp>

namespace poet
{
	namespace detail
	{
		template<typename T, typename Mutex, enum mutex_model>
		class specialized_monitor_ptr;

		struct static_cast_tag {};
		struct const_cast_tag {};
		struct dynamic_cast_tag {};

		// uses default copy constructor/assignment operators
		template<typename T, typename Mutex>
		class specialized_monitor_ptr<T, Mutex, mutex_concept>
		{
		public:
			typedef T element_type;
			typedef Mutex mutex_type;

			class scoped_lock: public detail::monitor_scoped_lock<T, Mutex>
			{
				typedef typename detail::monitor_scoped_lock<T, Mutex> base_class;
			public:
				scoped_lock(specialized_monitor_ptr<T, Mutex, mutex_concept> &monitor_pointer):
					base_class(monitor_pointer._syncer,
						monitor_pointer._pointer)
				{}
				explicit scoped_lock(const specialized_monitor_ptr<T, Mutex, mutex_concept> &monitor_pointer):
					base_class(monitor_pointer._syncer,
						monitor_pointer._pointer)
				{}
				scoped_lock(specialized_monitor_ptr<T, Mutex, mutex_concept> &monitor_pointer, bool do_lock):
					base_class(monitor_pointer._syncer, monitor_pointer._pointer, do_lock)
				{}
				explicit scoped_lock(const specialized_monitor_ptr<T, Mutex, mutex_concept> &monitor_pointer, bool do_lock):
					base_class(monitor_pointer._syncer, monitor_pointer._pointer, do_lock)
				{}
			};

			class call_proxy
			{
			public:
				const scoped_lock& operator->() {return *_lock;}
			private:
				template<typename U, typename M, enum mutex_model>
				friend class specialized_monitor_ptr;

				call_proxy(const boost::shared_ptr<scoped_lock> &lock):
					_lock(lock)
				{}

				boost::shared_ptr<scoped_lock> _lock;
			};

			specialized_monitor_ptr()
			{}
			specialized_monitor_ptr(const boost::shared_ptr<T> &smart_pointer): _pointer(smart_pointer),
				_syncer(new detail::monitor_synchronizer<Mutex>())
			{
				set_monitor_ptr(_pointer.get());
			}
			template<typename U>
			explicit specialized_monitor_ptr(U *pointer): _pointer(pointer),
				_syncer(new detail::monitor_synchronizer<Mutex>())
			{
				set_monitor_ptr(_pointer.get());
			}
			// support implicit conversions
			template<typename U>
			specialized_monitor_ptr(const specialized_monitor_ptr<U, Mutex, mutex_concept> &other): _pointer(other._pointer),
				_syncer(other._syncer)
			{
				set_monitor_ptr(_pointer.get());
			}
			// support static_pointer_cast
			template<typename U>
			specialized_monitor_ptr(const specialized_monitor_ptr<U, Mutex, mutex_concept> &other,
				static_cast_tag): _pointer(boost::static_pointer_cast<T>(other._pointer)),
				_syncer(other._syncer)
			{
				set_monitor_ptr(_pointer.get());
			}
			// support dynamic_pointer_cast
			template<typename U>
			specialized_monitor_ptr(const specialized_monitor_ptr<U, Mutex, mutex_concept> &other,
				dynamic_cast_tag): _pointer(boost::dynamic_pointer_cast<T>(other._pointer)),
				_syncer(other._syncer)
			{
				set_monitor_ptr(_pointer.get());
			}
			// support const_pointer_cast
			template<typename U>
			specialized_monitor_ptr(const specialized_monitor_ptr<U, Mutex, mutex_concept> &other,
				const_cast_tag): _pointer(boost::const_pointer_cast<T>(other._pointer)),
				_syncer(other._syncer)
			{
				set_monitor_ptr(_pointer.get());
			}

			virtual ~specialized_monitor_ptr() {}

			call_proxy operator->() const
			{
				return call_proxy(boost::shared_ptr<scoped_lock>(new scoped_lock(const_cast<specialized_monitor_ptr&>(*this))));
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
			template<typename U, typename M, enum mutex_model>
			friend class specialized_monitor_ptr;

			void set_monitor_ptr(const monitor_base *monitor)
			{
				monitor->set_synchronizer(_syncer);
			}
			void set_monitor_ptr(...)
			{}

			boost::shared_ptr<T> _pointer;
			boost::shared_ptr<detail::monitor_synchronizer<Mutex> > _syncer;
		};

		template<typename T, typename Mutex>
		class specialized_monitor_ptr<T, Mutex, try_mutex_concept>:
			public specialized_monitor_ptr<T, Mutex, mutex_concept>
		{
			typedef specialized_monitor_ptr<T, Mutex, mutex_concept> base_class;
		public:
			class scoped_try_lock: public detail::monitor_scoped_try_lock<T, Mutex>
			{
				typedef typename detail::monitor_scoped_try_lock<T, Mutex> base_class;
			public:
				scoped_try_lock(specialized_monitor_ptr<T, Mutex, mutex_concept> &monitor_pointer):
					base_class(monitor_pointer._syncer, monitor_pointer._pointer)
				{}
				explicit scoped_try_lock(const specialized_monitor_ptr<T, Mutex, mutex_concept> &monitor_pointer):
					base_class(monitor_pointer._syncer, monitor_pointer._pointer)
				{}
				scoped_try_lock(specialized_monitor_ptr<T, Mutex, mutex_concept> &monitor_pointer, bool do_lock):
					base_class(monitor_pointer._syncer, monitor_pointer._pointer, do_lock)
				{}
				explicit scoped_try_lock(const specialized_monitor_ptr<T, Mutex, mutex_concept> &monitor_pointer, bool do_lock):
					base_class(monitor_pointer._syncer, monitor_pointer._pointer, do_lock)
				{}
			};

			specialized_monitor_ptr()
			{}
			specialized_monitor_ptr(const boost::shared_ptr<T> &smart_pointer): base_class(smart_pointer)
			{}
			template<typename U>
			explicit specialized_monitor_ptr(U *pointer): base_class(pointer)
			{}
			template<typename U>
			specialized_monitor_ptr(const specialized_monitor_ptr<U, Mutex, try_mutex_concept> &other): base_class(other)
			{}
			template<typename U, typename CastTag>
			specialized_monitor_ptr(const specialized_monitor_ptr<U, Mutex, try_mutex_concept> &other,
				CastTag tag): base_class(other, tag)
			{}
		};

		template<typename T, typename Mutex>
		class specialized_monitor_ptr<T, Mutex, timed_mutex_concept>:
			public specialized_monitor_ptr<T, Mutex, try_mutex_concept>
		{
			typedef specialized_monitor_ptr<T, Mutex, try_mutex_concept> base_class;
		public:
			class scoped_timed_lock: public detail::monitor_scoped_timed_lock<T, Mutex>
			{
				typedef typename detail::monitor_scoped_timed_lock<T, Mutex> base_class;
			public:
				template<typename Timeout>
				scoped_timed_lock(specialized_monitor_ptr<T, Mutex, mutex_concept> &monitor_pointer, const Timeout &timeout):
					base_class(monitor_pointer._syncer, monitor_pointer._pointer, timeout)
				{}
				template<typename Timeout>
				explicit scoped_timed_lock(const specialized_monitor_ptr<T, Mutex, mutex_concept> &monitor_pointer, const Timeout &timeout):
					base_class(monitor_pointer._syncer, monitor_pointer._pointer, timeout)
				{}
				scoped_timed_lock(specialized_monitor_ptr<T, Mutex, mutex_concept> &monitor_pointer, bool do_lock):
					base_class(monitor_pointer._syncer, monitor_pointer._pointer, do_lock)
				{}
				explicit scoped_timed_lock(const specialized_monitor_ptr<T, Mutex, mutex_concept> &monitor_pointer, bool do_lock):
					base_class(monitor_pointer._syncer, monitor_pointer._pointer, do_lock)
				{}
			};

			specialized_monitor_ptr()
			{}
			specialized_monitor_ptr(const boost::shared_ptr<T> &smart_pointer): base_class(smart_pointer)
			{}
			template<typename U>
			explicit specialized_monitor_ptr(U *pointer): base_class(pointer)
			{}
			template<typename U>
			specialized_monitor_ptr(const specialized_monitor_ptr<U, Mutex, timed_mutex_concept> &other):
				base_class(other)
			{}
			template<typename U, typename CastTag>
			specialized_monitor_ptr(const specialized_monitor_ptr<U, Mutex, timed_mutex_concept> &other,
				CastTag tag): base_class(other, tag)
			{}
		};
	};

	template<typename T, typename Mutex = boost::mutex>
	class monitor_ptr: public detail::specialized_monitor_ptr<T, Mutex, mutex_properties<Mutex>::model>
	{
		typedef typename detail::specialized_monitor_ptr<T, Mutex, mutex_properties<Mutex>::model> base_class;
	public:
		monitor_ptr()
		{}
		monitor_ptr(const boost::shared_ptr<T> &smart_pointer): base_class(smart_pointer)
		{}
		template<typename U>
		explicit monitor_ptr(U *pointer): base_class(pointer)
		{}
		template<typename U>
		monitor_ptr(const monitor_ptr<U, Mutex> &other): base_class(other)
		{}
		template<typename U, typename CastTag>
		monitor_ptr(const monitor_ptr<U, Mutex> &other, CastTag tag): base_class(other, tag)
		{}
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
		return monitor_ptr<T, Mutex>(pointer, detail::static_cast_tag());
	}
	template<typename T, typename U, typename Mutex>
	inline monitor_ptr<T, Mutex> dynamic_pointer_cast(const monitor_ptr<U, Mutex> &pointer)
	{
		return monitor_ptr<T, Mutex>(pointer, detail::dynamic_cast_tag());
	}
	template<typename T, typename U, typename Mutex>
	inline monitor_ptr<T, Mutex> const_pointer_cast(const monitor_ptr<U, Mutex> &pointer)
	{
		return monitor_ptr<T, Mutex>(pointer, detail::const_cast_tag());
	}
};

#endif // _POET_MONITOR_PTR_HPP
