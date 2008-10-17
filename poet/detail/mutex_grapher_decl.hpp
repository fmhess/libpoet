/*
	A thread-safe singleton class which can be used build up a graph of a
	program's locking order for mutexes, and check it for locking
	order violations.

	begin: Frank Mori Hess <fmhess@users.sourceforge.net>  2007-10-26
	copyright (c) Frank Mori Hess 2007-2008
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MUTEX_GRAPHER_DECL_HPP
#define _POET_MUTEX_GRAPHER_DECL_HPP

#include <boost/function.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/optional.hpp>
#include <boost/thread/once.hpp>
#include <boost/thread/tss.hpp>
#include <boost/noncopyable.hpp>
#include <cassert>
#include <list>
#include <map>
#include <poet/detail/template_static.hpp>
#include <poet/monitor_ptr.hpp>
#include <poet/monitor_locks.hpp>
#include <string>

namespace poet
{
	// forward declarations
	class acyclic_mutex_base;
	template<typename Mutex, typename Key, typename KeyCompare>
	class acyclic_mutex;
	namespace detail
	{
		template<typename AcyclicMutex>
		class acyclic_scoped_lock;

		template<typename AssociatedClass> class template_static_once_flag
		{
		public:
			static boost::once_flag flag;
		};
		template<typename AssociatedClass>
			boost::once_flag template_static_once_flag<AssociatedClass>::flag = BOOST_ONCE_INIT;
	};

	class mutex_grapher: public boost::noncopyable
	{
		typedef monitor_ptr<mutex_grapher, boost::mutex> monitor_type;
	public:
		struct vertex_properties
		{
			vertex_properties(): use_count(0) {}
			std::string name;
			int use_count;
		};
		struct edge_properties
		{
			edge_properties(): locking_order_violation(false) {}
			bool locking_order_violation;
		};
		typedef boost::adjacency_list<boost::setS, boost::vecS, boost::bidirectionalS, vertex_properties, edge_properties>
			locking_order_graph;
		typedef std::list<const acyclic_mutex_base *> mutex_list_type;
		class unique_lock: public monitor_unique_lock<monitor_type>
		{
		public:
			unique_lock(): monitor_unique_lock<monitor_type>(mutex_grapher::instance())
			{}
		};
		typedef unique_lock scoped_lock;

		const locking_order_graph& graph() const {return _graph;}
		inline void write_graphviz(std::ostream &out_stream) const;
		template<typename Func>
		void set_cycle_handler(Func func)
		{
			_cycle_handler = func;
		}
		const mutex_list_type& locked_mutexes() const
		{
			check_locked_mutexes_init();
			return *_locked_mutexes;
		}
	private:
		class vertex_labeler
		{
		public:
			vertex_labeler(const locking_order_graph &graph): _graph(graph)
			{}
			void operator()(std::ostream &output_stream, locking_order_graph::vertex_descriptor vertex)
			{
				output_stream << "[label=\"" << _graph[vertex].name << "\"]";
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
				output_stream << "[color=\"";
				if(_graph[edge].locking_order_violation)
				{
					output_stream << "red";
				}else
				{
					output_stream << "black";
				}
				output_stream << "\"]";
			}
		private:
			const locking_order_graph &_graph;
		};
		template<typename Mutex, bool recursive, enum mutex_model model, typename Key, typename KeyCompare>
		friend class detail::specialized_acyclic_mutex;
		template<typename Mutex, typename Key, typename KeyCompare>
		friend class acyclic_mutex;

		inline mutex_grapher();

		void check_locked_mutexes_init() const
		{
			if(_locked_mutexes.get() == 0)
			{
				_locked_mutexes.reset(new mutex_list_type);
			}
		}
		mutex_list_type& internal_locked_mutexes()
		{
			check_locked_mutexes_init();
			return *_locked_mutexes;
		}
		template<typename AcyclicMutex>
		inline bool track_lock(AcyclicMutex &_mutex);
		inline void track_unlock(const acyclic_mutex_base &_mutex);
		inline void check_for_cycles() const;
		template<typename AcyclicMutex>
		inline void acquire_vertex(AcyclicMutex &mutex);
		template<typename AcyclicMutex>
		inline void release_vertex(const AcyclicMutex &mutex);

		// static functions
		static monitor_type& instance()
		{
			static monitor_type *grapher;
			boost::call_once(detail::template_static_once_flag<mutex_grapher>::flag,
				boost::bind(&mutex_grapher::init_grapher_instance, &grapher));
			return *grapher;
		}
		static void init_grapher_instance(monitor_type **grapher)
		{
			*grapher = new monitor_type(new mutex_grapher);
		}
		inline static void default_cycle_handler();

		locking_order_graph _graph;
		bool _cycle_detected;
		mutable boost::thread_specific_ptr<mutex_list_type> _locked_mutexes;
		boost::function<void ()> _cycle_handler;
		std::vector<locking_order_graph::vertex_descriptor> _free_vertices;
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
			void remove_vertex(const Key &key)
			{
				_vertex_map.erase(key);
			}
		private:
			vertex_finder() {}

			// static functions
			static monitor_type& instance()
			{
				static monitor_type *finder;

				boost::call_once(_instance_init_flag,
					boost::bind(&vertex_finder::init_finder_instance, &finder));

				return *finder;
			}
			static void init_finder_instance(monitor_type **finder)
			{
				*finder = new monitor_type(new vertex_finder);
			}

			vertex_map_type _vertex_map;
			static boost::once_flag _instance_init_flag;
		};
		template<typename Key, typename KeyCompare>
			boost::once_flag vertex_finder<Key, KeyCompare>::_instance_init_flag;
	};
};

#endif // _POET_MUTEX_GRAPHER_DECL_HPP
