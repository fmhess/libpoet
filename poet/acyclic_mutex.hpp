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
#include <boost/optional.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/tss.hpp>
#include <cassert>
#include <functional>
#include <poet/acyclic_mutex_base.hpp>
#include <poet/mutex_grapher.hpp>
#include <poet/mutex_properties.hpp>
#include <sstream>
#include <string>

namespace poet
{
	namespace detail
	{
		template<typename AcyclicMutex>
		class acyclic_scoped_lock: public boost::unique_lock<AcyclicMutex>
		{
		public:
			acyclic_scoped_lock(AcyclicMutex &mutex):
				boost::unique_lock<AcyclicMutex>(mutex, boost::defer_lock_t())
			{
				this->lock();
			}
			acyclic_scoped_lock(AcyclicMutex &mutex, bool do_lock):
				boost::unique_lock<AcyclicMutex>(mutex, boost::defer_lock_t())
			{
				if(do_lock)
				{
					this->lock();
				}
			}
			bool locked() const {return this->owns_lock();}
		};

		template<typename AcyclicMutex>
		class acyclic_scoped_try_lock: public acyclic_scoped_lock<AcyclicMutex>
		{
			typedef acyclic_scoped_lock<AcyclicMutex> base_class;
		public:
			acyclic_scoped_try_lock(AcyclicMutex &mutex): base_class(mutex, false)
			{
				this->try_lock();
			}
			acyclic_scoped_try_lock(AcyclicMutex &mutex, bool do_lock): base_class(mutex, do_lock)
			{}
		};

		template<typename AcyclicMutex>
		class acyclic_scoped_timed_lock: public acyclic_scoped_try_lock<AcyclicMutex>
		{
			typedef acyclic_scoped_try_lock<AcyclicMutex> base_class;
		public:
			template<typename Timeout>
			acyclic_scoped_timed_lock(AcyclicMutex &mutex, const Timeout &t): base_class(mutex, false)
			{
				this->timed_lock(t);
			}
			acyclic_scoped_timed_lock(AcyclicMutex &mutex, bool do_lock): base_class(mutex, do_lock)
			{}
		};

		template<typename Mutex, bool recursive, enum mutex_model model, typename Key, typename KeyCompare>
		class specialized_acyclic_mutex;

#ifdef ACYCLIC_MUTEX_NDEBUG	// user is compiling with lock order debugging disabled
		template<typename Mutex, bool recursive, enum mutex_model model, typename Key, typename KeyCompare>
		class specialized_acyclic_mutex: public acyclic_mutex_base, public Mutex
		{
		public:
			typedef Mutex mutex_type;
			typedef Key key_type;
			typedef KeyCompare key_compare;

			specialized_acyclic_mutex()
			{}
			specialized_acyclic_mutex(const Key &node_key)
			{}
			boost::optional<Key> node_key() const {return boost::optional<Key>();}
		};
#else // ACYCLIC_MUTEX_NDEBUG undefined

		// non-recursive mutex
		template<typename Mutex, typename Key, typename KeyCompare>
		class specialized_acyclic_mutex<Mutex, false, Lockable, Key, KeyCompare>:
			public acyclic_mutex_base
		{
		public:
			typedef Mutex mutex_type;
			typedef Key key_type;
			typedef KeyCompare key_compare;
			typedef detail::acyclic_scoped_lock<specialized_acyclic_mutex> scoped_lock;
			typedef detail::acyclic_scoped_try_lock<specialized_acyclic_mutex> scoped_try_lock;

			specialized_acyclic_mutex()
			{}
			specialized_acyclic_mutex(const Key &node_key): _node_key(node_key)
			{}

			boost::optional<Key> node_key() const {return _node_key;}

			// Boost.Threads interface for Lockable concepts
			// Lockable
			void lock()
			{
				track_lock();
				_wrapped_mutex.lock();
			}
			bool try_lock()
			{
				track_lock();
				bool successful = _wrapped_mutex.try_lock();
				if(successful == false) track_unlock();
				return successful;
			}
			void unlock()
			{
				_wrapped_mutex.unlock();
				track_unlock();
			}
#if 0	//TODO
			// SharedLockable
			void lock_shared() {}
			bool try_lock_shared() {}
			template<typename Timeout>
			bool timed_lock_shared(const Timeout &timeout) {}
			void unlock_shared() {}
			// UpgradeLockable
			void lock_upgrade() {}
			void unlock_upgrade() {}
			void unlock_upgrade_and_lock() {}
			void unlock_upgrade_and_lock_shared() {}
			void unlock_and_lock_upgrade() {}
#endif
		protected:
			boost::optional<Key> _node_key;
			Mutex _wrapped_mutex;
		private:
			void track_lock()
			{
				bool cycle_detected;
				{
					mutex_grapher::scoped_lock lock;
					cycle_detected = lock->track_lock(*this);
				};
				/* _cycle_handler is run with no locks held by libpoet,
				to minimize chance of deadlock with user-provided cycle handler.
				tracking of locking events is disabled after the first cycle
				is detected. */
				if(cycle_detected) mutex_grapher::instance().direct()->_cycle_handler();
			}
			void track_unlock()
			{
				mutex_grapher::scoped_lock lock;
				lock->track_unlock(*this);
			}
		};

		// recursive mutex
		template<typename Mutex, typename Key, typename KeyCompare>
		class specialized_acyclic_mutex<Mutex, true, Lockable, Key, KeyCompare>:
			public specialized_acyclic_mutex<Mutex, false, Lockable, Key, KeyCompare>
		{
			typedef specialized_acyclic_mutex<Mutex, false, Lockable, Key, KeyCompare> base_class;
		public:
			specialized_acyclic_mutex()
			{}
			specialized_acyclic_mutex(const Key &node_key): base_class(node_key)
			{}
			~specialized_acyclic_mutex() {}

			void lock()
			{
				track_lock();
				this->_wrapped_mutex.lock();
			}
			bool try_lock()
			{
				track_lock();
				bool successful = this->_wrapped_mutex.try_lock();
				if(successful == false) track_unlock();
				return successful;
			}
			void unlock()
			{
				this->_wrapped_mutex.unlock();
				track_unlock();
			}
		private:
			void check_lock_count_init() const
			{
				if(_lock_count.get() == 0) _lock_count.reset(new int(0));
			}
			void increment_recursive_lock_count()
			{
				check_lock_count_init();
				++(*_lock_count);
				assert(*_lock_count >= 0);
			}
			void decrement_recursive_lock_count()
			{
				check_lock_count_init();
				--(*_lock_count);
				assert(*_lock_count >= 0);
			}
			int get_recursive_lock_count() const
			{
				check_lock_count_init();
				return *_lock_count;
			}
			void track_lock()
			{
				if(get_recursive_lock_count() == 0)
				{
					base_class::track_lock();
				}
				increment_recursive_lock_count();
			}
			void track_unlock()
			{
				decrement_recursive_lock_count();
				if(get_recursive_lock_count() == 0)
				{
					base_class::track_unlock();
				}
			}

			mutable boost::thread_specific_ptr<int> _lock_count;
		};

		// timed mutex
		template<typename Mutex, bool recursive, typename Key, typename KeyCompare>
		class specialized_acyclic_mutex<Mutex, recursive, TimedLockable, Key, KeyCompare>:
			public specialized_acyclic_mutex<Mutex, recursive, Lockable, Key, KeyCompare>
		{
			typedef specialized_acyclic_mutex<Mutex, recursive, Lockable, Key, KeyCompare> base_class;
		public:
			typedef detail::acyclic_scoped_timed_lock<specialized_acyclic_mutex> scoped_timed_lock;

			specialized_acyclic_mutex()
			{}
			specialized_acyclic_mutex(const Key &node_key): base_class(node_key)
			{}

			// TimedLockable
			template<typename Timeout>
			bool timed_lock(const Timeout &timeout)
			{
				base_class::track_lock();
				bool successful = this->_wrapped_mutex.try_lock(timeout);
				if(successful == false) base_class::track_unlock();
				return successful;
			}
		private:
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
		~acyclic_mutex()
		{
			mutex_grapher::scoped_lock lock;
			lock->release_vertex(*this);
		}
	};
};

#endif // _POET_ACYCLIC_MUTEX_HPP
