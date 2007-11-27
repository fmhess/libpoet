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
#include <poet/detail/acyclic_mutex_base.hpp>
#include <poet/mutex_grapher.hpp>
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
			mutex_grapher::tracker _tracker;
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
	};

#ifdef ACYCLIC_MUTEX_NDEBUG	// user is compiling with lock order debugging disabled
	template<typename Key = std::string, typename Mutex = boost::mutex>
	class acyclic_mutex: public detail::acyclic_mutex_base, public Mutex
	{
	public:
		typedef Mutex wrapped_mutex_type;
		typedef Key key_type;

		acyclic_mutex(const Key &node_key = Key()): detail::acyclic_mutex_base(node_key)
		{}
	};

	template<typename Key = std::string, typename Mutex = boost::try_mutex>
	class acyclic_try_mutex: public detail::acyclic_mutex_base, public Mutex
	{
	public:
		acyclic_try_mutex(const Key &node_key = Key()): detail::acyclic_mutex_base(node_key)
		{}
	};

	template<typename Key = std::string, typename Mutex = boost::timed_mutex>
	class acyclic_timed_mutex: public detail::acyclic_mutex_base, public Mutex
	{
	public:
		acyclic_timed_mutex(const Key &node_key = Key()): detail::acyclic_mutex_base(node_key)
		{}
	};
#else // ACYCLIC_MUTEX_NDEBUG undefined
	template<typename Key = std::string, typename Mutex = boost::mutex>
	class acyclic_mutex: public detail::acyclic_mutex_base
	{
	public:
		typedef Mutex wrapped_mutex_type;
		typedef Key key_type;
		typedef detail::acyclic_scoped_lock<acyclic_mutex<Key, Mutex> > scoped_lock;

		acyclic_mutex(const Key &node_key = Key()): detail::acyclic_mutex_base(node_key)
		{}
	protected:
		template<typename T>
		friend class detail::acyclic_scoped_lock;

		Mutex _wrapped_mutex;
	};

	template<typename Key = std::string, typename Mutex = boost::try_mutex>
	class acyclic_try_mutex: public acyclic_mutex<Key, Mutex>
	{
	public:
		typedef detail::acyclic_scoped_try_lock<acyclic_mutex<Key, Mutex> > scoped_try_lock;

		acyclic_try_mutex(const Key &node_key = Key()): acyclic_mutex<Key, Mutex>(node_key)
		{}
	protected:
		template<typename T>
		friend class detail::acyclic_scoped_try_lock;
	};

	template<typename Key = std::string, typename Mutex = boost::timed_mutex>
	class acyclic_timed_mutex: public acyclic_try_mutex<Key, Mutex>
	{
	public:
		typedef detail::acyclic_scoped_timed_lock<acyclic_mutex<Key, Mutex> > scoped_timed_lock;

		acyclic_timed_mutex(const Key &node_key = Key()): acyclic_try_mutex<Key, Mutex>(node_key)
		{}
	protected:
		template<typename T>
		friend class detail::acyclic_scoped_timed_lock;
	};
#endif	// ACYCLIC_MUTEX_NDEBUG
};

#endif // _POET_ACYCLIC_MUTEX_HPP
