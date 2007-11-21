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

#include <boost/graph/adjacency_list.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/tss.hpp>
#include <cassert>
#include <list>
#include <map>
#include <poet/detail/static_mutex.hpp>
#include <string>

namespace poet
{
	namespace detail
	{
		class acyclic_mutex_base;
	};

	class mutex_grapher
	{
	public:
		struct vertex_properties
		{
			std::string name;
		};
		enum edge_status
		{
			EDGE_OK,
			EDGE_LOCKING_ORDER_VIOLATION
		};
		struct edge_properties
		{
			edge_properties(): status(EDGE_OK) {}
			enum edge_status status;
		};
		typedef boost::adjacency_list<boost::setS, boost::vecS, boost::directedS, vertex_properties, edge_properties>
			locking_order_graph;
		typedef std::map<std::string, locking_order_graph::vertex_descriptor> vertex_map_type;

		static mutex_grapher& instance()
		{
			static mutex_grapher *grapher = 0;

			boost::mutex::scoped_lock lock(detail::static_mutex<mutex_grapher>::mutex);
			if(grapher == 0) grapher = new mutex_grapher();
			return *grapher;
		}
		const locking_order_graph& graph() const {return _graph;}
		inline void write_graphviz(std::ostream &out_stream);
  private:
		class tracker
		{
		public:
			tracker(const detail::acyclic_mutex_base &mutex): _tracking(false), _mutex(mutex)
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
				mutex_grapher::instance().track_lock(_mutex);
			}
			void track_unlock()
			{
				if(_tracking == false) return;
				_tracking = false;
				mutex_grapher::instance().track_unlock(_mutex);
			}

		private:
			bool _tracking;
			const detail::acyclic_mutex_base &_mutex;
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
				switch(_graph[edge].status)
				{
				case EDGE_OK:
					output_stream << "black";
					break;
				case EDGE_LOCKING_ORDER_VIOLATION:
					output_stream << "red";
					break;
				default:
					assert(false);
					break;
				}
				output_stream << "]";
			}
		private:
			const locking_order_graph &_graph;
		};
		friend class tracker;
		typedef std::list<const detail::acyclic_mutex_base *> mutex_list_type;

		mutex_grapher()
		{}
		mutex_list_type& locked_mutexes()
		{
			if(_locked_mutexes.get() == 0)
			{
				_locked_mutexes.reset(new mutex_list_type);
			}
			return *_locked_mutexes;
		}
		inline void track_lock(const detail::acyclic_mutex_base &_mutex);
		inline void track_unlock(const detail::acyclic_mutex_base &_mutex);
		inline void check_for_cycles() const;

		locking_order_graph _graph;
		vertex_map_type _vertex_map;
		boost::thread_specific_ptr<mutex_list_type> _locked_mutexes;
		boost::mutex _graph_mutex;
	};
};

#endif // _POET_MUTEX_GRAPHER_DECL_HPP
