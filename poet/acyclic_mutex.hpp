/*
	A mutex wrapper which automatically tracks locking order to insure no deadlocks
	are possible.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-10-26
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_ACYCLIC_MUTEX_HPP
#define _POET_ACYCLIC_MUTEX_HPP

#include <boost/graph/graphviz.hpp>
#include <boost/thread/mutex.hpp>
#include <cassert>
#include <functional>
#include <poet/detail/acyclic_mutex_base.hpp>
#include <poet/mutex_grapher.hpp>
#include <poet/mutex_properties.hpp>
#include <sstream>
#include <string>

namespace poet
{
	namespace detail
	{
		template<typename AcyclicMutex, typename Lock = typename AcyclicMutex::wrapped_mutex_type::scoped_lock>
		class acyclic_scoped_lock
		{
		public:
			acyclic_scoped_lock(AcyclicMutex &mutex): _tracker(mutex),
				_lock(mutex._wrapped_mutex)
			{
				_tracker.track_lock();
			}
			acyclic_scoped_lock(AcyclicMutex &mutex, bool do_lock):
				_tracker(mutex),
				_lock(mutex._wrapped_mutex, false)
			{
				if(do_lock)
				{
					_tracker.track_lock();
					_lock.lock();
				}
			}
			bool locked() const {return _lock.locked();}
			operator const void*() const {return static_cast<const void*>(_lock);}
			void lock()
			{
				_tracker.track_lock();
				_lock.lock();
			}
			void unlock()
			{
				_tracker.track_unlock();
				_lock.unlock();
			}
		protected:
			mutex_grapher::tracker<AcyclicMutex> _tracker;
			Lock _lock;
		};

		template<typename AcyclicMutex, typename Lock = typename AcyclicMutex::wrapped_mutex_type::scoped_try_lock>
		class acyclic_scoped_try_lock: public acyclic_scoped_lock<AcyclicMutex, Lock>
		{
			typedef acyclic_scoped_lock<AcyclicMutex, Lock> base_class;
		public:
			acyclic_scoped_try_lock(AcyclicMutex &mutex): base_class(mutex, false)
			{
				try_lock();
			}
			acyclic_scoped_try_lock(AcyclicMutex &mutex, bool do_lock): base_class(mutex, do_lock)
			{}
			bool try_lock()
			{
				this->_tracker.track_lock();
				bool locked = this->_lock.try_lock();
				if(locked == false)
				{
					this->_tracker.track_unlock();
				}
				return locked;
			}
		};

		template<typename AcyclicMutex, typename Lock = typename AcyclicMutex::wrapped_mutex_type::scoped_timed_lock>
		class acyclic_scoped_timed_lock: public acyclic_scoped_try_lock<AcyclicMutex, Lock>
		{
			typedef acyclic_scoped_try_lock<AcyclicMutex, Lock> base_class;
		public:
			template<typename Timeout>
			acyclic_scoped_timed_lock(AcyclicMutex &mutex, const Timeout &t): base_class(mutex, false)
			{
				timed_lock(t);
			}
			acyclic_scoped_timed_lock(AcyclicMutex &mutex, bool do_lock): base_class(mutex, do_lock)
			{}
			template<typename Timeout>
			bool timed_lock(const Timeout &t)
			{
				this->_tracker.track_lock();
				bool locked = this->lock.timed_lock(t);
				if(locked == false)
				{
					this->_tracker.track_unlock();
				}
				return locked;
			}
		};

		template<typename Mutex, bool recursive, enum mutex_model model, typename Key, typename KeyCompare>
		class specialized_acyclic_mutex;

#ifdef ACYCLIC_MUTEX_NDEBUG	// user is compiling with lock order debugging disabled
		template<typename Mutex, bool recursive, enum mutex_model model, typename Key, typename KeyCompare>
		class specialized_acyclic_mutex: public detail::acyclic_mutex_keyed_base<Key>, public Mutex
		{
		public:
			typedef Mutex wrapped_mutex_type;
			typedef Key key_type;
			typedef KeyCompare key_compare;

			specialized_acyclic_mutex()
			{}
			specialized_acyclic_mutex(const Key &node_key): detail::acyclic_mutex_keyed_base<Key>(node_key)
			{}
		};
#else // ACYCLIC_MUTEX_NDEBUG undefined
		template<typename Mutex, bool recursive, typename Key, typename KeyCompare>
		class specialized_acyclic_mutex<Mutex, recursive, mutex_concept, Key, KeyCompare>:
			public detail::acyclic_mutex_keyed_base<Key>
		{
		public:
			typedef Mutex wrapped_mutex_type;
			typedef Key key_type;
			typedef KeyCompare key_compare;
			typedef detail::acyclic_scoped_lock<specialized_acyclic_mutex> scoped_lock;

			specialized_acyclic_mutex()
			{}
			specialized_acyclic_mutex(const Key &node_key): detail::acyclic_mutex_keyed_base<Key>(node_key)
			{}
		protected:
			template<typename M, typename L>
			friend class detail::acyclic_scoped_lock;

			Mutex _wrapped_mutex;
		};

		template<typename Mutex, bool recursive, typename Key, typename KeyCompare>
		class specialized_acyclic_mutex<Mutex, recursive, try_mutex_concept, Key, KeyCompare>:
			public specialized_acyclic_mutex<Mutex, recursive, mutex_concept, Key, KeyCompare>
		{
			typedef specialized_acyclic_mutex<Mutex, recursive, mutex_concept, Key, KeyCompare> base_class;
		public:
			typedef detail::acyclic_scoped_try_lock<specialized_acyclic_mutex> scoped_try_lock;

			specialized_acyclic_mutex()
			{}
			specialized_acyclic_mutex(const Key &node_key): base_class(node_key)
			{}
		protected:
			template<typename M, typename L>
			friend class detail::acyclic_scoped_try_lock;
		};

		template<typename Mutex, bool recursive, typename Key, typename KeyCompare>
		class specialized_acyclic_mutex<Mutex, recursive, timed_mutex_concept, Key, KeyCompare>:
			public specialized_acyclic_mutex<Mutex, recursive, try_mutex_concept, Key, KeyCompare>
		{
			typedef specialized_acyclic_mutex<Mutex, recursive, try_mutex_concept, Key, KeyCompare> base_class;
		public:
			typedef detail::acyclic_scoped_timed_lock<specialized_acyclic_mutex> scoped_timed_lock;

			specialized_acyclic_mutex()
			{}
			specialized_acyclic_mutex(const Key &node_key): base_class(node_key)
			{}
		protected:
			template<typename M, typename L>
			friend class detail::acyclic_scoped_timed_lock;
		};
#endif	// ACYCLIC_MUTEX_NDEBUG
	};

	template<typename Mutex = boost::mutex, typename Key = std::string, typename KeyCompare = std::less<Key> >
	class acyclic_mutex:
		public detail::specialized_acyclic_mutex<Mutex, mutex_properties<Mutex>::recursive,
			mutex_properties<Mutex>::model, Key, KeyCompare>
	{
		typedef typename detail::specialized_acyclic_mutex<Mutex, mutex_properties<Mutex>::recursive,
			mutex_properties<Mutex>::model, Key, KeyCompare> base_class;
	public:
		acyclic_mutex()
		{}
		acyclic_mutex(const Key &node_key): base_class(node_key)
		{}
	};
};

#endif // _POET_ACYCLIC_MUTEX_HPP
