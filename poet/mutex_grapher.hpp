/*
	A thread-safe singleton class which can be used build up a graph of a
	program's locking order for mutexes, and check it for locking
	order violations.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-10-26
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MUTEX_GRAPHER_HPP
#define _POET_MUTEX_GRAPHER_HPP

#include <boost/function.hpp>
#include <boost/graph/adjacency_list.hpp>
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
		class acyclic_mutex_base;
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
		typedef std::map<std::string, locking_order_graph::vertex_descriptor> vertex_map_type;
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
				mutex_grapher::scoped_lock lock;
				lock->track_lock(_mutex);
			}
			void track_unlock()
			{
				if(_tracking == false) return;
				_tracking = false;
				mutex_grapher::scoped_lock lock;
				lock->track_unlock(_mutex);
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
		inline void track_lock(const detail::acyclic_mutex_base &_mutex);
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
		vertex_map_type _vertex_map;
		boost::thread_specific_ptr<mutex_list_type> _locked_mutexes;
		boost::function<void ()> _cycle_handler;
	};
};

#include <poet/detail/mutex_grapher.ipp>

#endif // _POET_MUTEX_GRAPHER_HPP
