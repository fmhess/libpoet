/*
	A thread-safe singleton class which can be used build up a graph of a
	program's locking order for mutexes, and check it for locking
	order violations.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-10-26
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MUTEX_GRAPHER_DECL_HPP
#define _POET_MUTEX_GRAPHER_DECL_HPP

#include <boost/function.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/optional.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/tss.hpp>
#include <cassert>
#include <list>
#include <map>
#include <poet/detail/template_static.hpp>
#include <poet/monitor.hpp>
#include <string>

namespace poet
{
	namespace detail
	{
		// forward declarations
		class acyclic_mutex_base;
		template<typename Key>
		class acyclic_mutex_keyed_base;
		class acyclic_mutex_base;
		template<typename AcyclicMutex, typename Lock>
		class acyclic_scoped_lock;
	};

	class mutex_grapher
	{
		typedef monitor_ptr<mutex_grapher, boost::recursive_mutex> monitor_type;
	public:
		struct vertex_properties
		{
			std::string name;
		};
		struct edge_properties
		{
			edge_properties(): locking_order_violation(false) {}
			bool locking_order_violation;
		};
		typedef boost::adjacency_list<boost::setS, boost::vecS, boost::directedS, vertex_properties, edge_properties>
			locking_order_graph;
		class scoped_lock: public monitor_type::scoped_lock
		{
		public:
			scoped_lock(): monitor_type::scoped_lock(mutex_grapher::instance())
			{}
		};

		const locking_order_graph& graph() const {return _graph;}
		inline void write_graphviz(std::ostream &out_stream);
		template<typename Func>
		void set_cycle_handler(Func func)
		{
			_cycle_handler = func;
		}
	private:
		template<typename AcyclicMutex>
		class tracker
		{
		public:
			tracker(AcyclicMutex &mutex): _tracking(false), _mutex(mutex)
			{
			}
			~tracker()
			{
				track_unlock();
			}
			void track_lock()
			{
				if(_tracking) return;
				_tracking = true;
				mutex_grapher::scoped_lock lock;
				lock->track_lock(_mutex);
				_mutex.increment_recursive_lock_count();
			}
			void track_unlock()
			{
				if(_tracking == false) return;
				_tracking = false;
				_mutex.decrement_recursive_lock_count();
				mutex_grapher::scoped_lock lock;
				lock->track_unlock(_mutex);
			}

		private:
			bool _tracking;
			AcyclicMutex &_mutex;
		};
		class vertex_labeler
		{
		public:
			vertex_labeler(const locking_order_graph &graph): _graph(graph)
			{}
			void operator()(std::ostream &output_stream, locking_order_graph::vertex_descriptor vertex)
			{
				output_stream << "[label=" << _graph[vertex].name << "]";
			}
		private:
			const locking_order_graph &_graph;
		};
		class edge_colorer
		{
		public:
			edge_colorer(const locking_order_graph &graph): _graph(graph)
			{}
			void operator()(std::ostream &output_stream, locking_order_graph::edge_descriptor edge)
			{
				output_stream << "[color=";
				if(_graph[edge].locking_order_violation)
				{
					output_stream << "red";
				}else
				{
					output_stream << "black";
				}
				output_stream << "]";
			}
		private:
			const locking_order_graph &_graph;
		};
		template<typename AcyclicMutex, typename Lock>
		friend class detail::acyclic_scoped_lock;
		template<typename AcyclicMutex>
		friend class tracker;
		typedef std::list<const detail::acyclic_mutex_base *> mutex_list_type;

		inline mutex_grapher();
		mutex_list_type& locked_mutexes()
		{
			if(_locked_mutexes.get() == 0)
			{
				_locked_mutexes.reset(new mutex_list_type);
			}
			return *_locked_mutexes;
		}
		template<typename AcyclicMutex>
		inline void track_lock(AcyclicMutex &_mutex);
		inline void track_unlock(const detail::acyclic_mutex_base &_mutex);
		inline void check_for_cycles() const;

		// static functions
		static monitor_type& instance()
		{
			static monitor_type grapher;

			boost::mutex::scoped_lock lock(detail::template_static<mutex_grapher, boost::mutex>::object);
			if(grapher == 0) grapher.reset(new mutex_grapher);
			return grapher;
		}
		inline static void default_cycle_handler();

		locking_order_graph _graph;
		boost::thread_specific_ptr<mutex_list_type> _locked_mutexes;
		boost::function<void ()> _cycle_handler;
	};

	namespace detail
	{
		template<typename Key, typename KeyCompare>
		class vertex_finder
		{
			typedef monitor_ptr<vertex_finder, boost::mutex> monitor_type;
		public:
			typedef Key key_type;
			typedef KeyCompare key_compare;
			typedef mutex_grapher::locking_order_graph::vertex_descriptor vertex_descriptor_type;
			typedef std::map<Key, vertex_descriptor_type, KeyCompare> vertex_map_type;
			class scoped_lock: public monitor_type::scoped_lock
			{
			public:
				scoped_lock(): monitor_type::scoped_lock(vertex_finder::instance())
				{}
			};

			const vertex_descriptor_type* find_vertex(const Key &key) const
			{
				typename vertex_map_type::const_iterator it = _vertex_map.find(key);
				if(it == _vertex_map.end())
				{
					return 0;
				}else
				{
					return &it->second;
				}
			}
			void add_vertex(const Key &key, vertex_descriptor_type vertex)
			{
				_vertex_map[key] = vertex;
			}
		private:
			vertex_finder() {}

			// static functions
			static monitor_type& instance()
			{
				static monitor_type finder;

				boost::mutex::scoped_lock lock(_singleton_mutex);
				if(finder == 0) finder.reset(new vertex_finder);
				return finder;
			}

			vertex_map_type _vertex_map;
			static boost::mutex _singleton_mutex;
		};
		template<typename Key, typename KeyCompare>
		boost::mutex vertex_finder<Key, KeyCompare>::_singleton_mutex;
	};
};

#endif // _POET_MUTEX_GRAPHER_DECL_HPP
