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

#ifndef _POET_MUTEX_GRAPHER_IPP
#define _POET_MUTEX_GRAPHER_IPP

#include <poet/mutex_grapher.hpp>

#include <boost/graph/topological_sort.hpp>
#include <boost/graph/graphviz.hpp>
#include <cassert>
#include <cstdlib>
#include <list>
#include <map>
#include <poet/acyclic_mutex_base.hpp>
#include <poet/mutex_properties.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace poet
{
	mutex_grapher::mutex_grapher(): _cycle_detected(false),
		_cycle_handler(&default_cycle_handler)
	{}

	template<typename AcyclicMutex>
	bool mutex_grapher::track_lock(AcyclicMutex &mutex)
	{
		// we want to ignore all locking events after the first cycle is detected.
		if(_cycle_detected) return false;

		if(mutex.vertex() == 0)
		{
			acquire_vertex(mutex);
		}
		if(locked_mutexes().empty() == false)
		{
			const typename locking_order_graph::vertex_descriptor source_vertex = *locked_mutexes().back()->vertex();
			typename locking_order_graph::edge_descriptor new_edge = boost::add_edge(source_vertex,
				*mutex.vertex(), _graph).first;
			try
			{
				check_for_cycles();
			}
			catch(const boost::not_a_dag &)
			{
				_cycle_detected = true;
				_graph[new_edge].locking_order_violation = true;
			}
		}
		internal_locked_mutexes().push_back(&mutex);
		return _cycle_detected;
	};

	void mutex_grapher::track_unlock(const acyclic_mutex_base &mutex)
	{
		// we want to ignore all locking events after the first cycle is detected.
		if(_cycle_detected) return;

		mutex_list_type::reverse_iterator rit = std::find(internal_locked_mutexes().rbegin(), internal_locked_mutexes().rend(), &mutex);
		assert(rit != internal_locked_mutexes().rend());
		internal_locked_mutexes().erase(--(rit.base()));
	}

	void mutex_grapher::write_graphviz(std::ostream &out_stream) const
	{
		boost::write_graphviz(out_stream, graph(), vertex_labeler(graph()), edge_colorer(graph()));
	}

	void mutex_grapher::check_for_cycles() const
	{
		typedef std::vector<locking_order_graph::vertex_descriptor> result_type;
		result_type result;
		// throws boost::not_a_dag if graph has cycles
		boost::topological_sort(_graph, std::back_inserter(result));
	}

	void mutex_grapher::default_cycle_handler()
	{
		std::cerr << "mutex_grapher::default_cycle_handler(): cycle detected in mutex locking order." << std::endl;
		std::abort();
	}

	template<typename AcyclicMutex>
	void mutex_grapher::acquire_vertex(AcyclicMutex &mutex)
	{
		typedef detail::vertex_finder<typename AcyclicMutex::key_type, typename AcyclicMutex::key_compare> vertex_finder_type;
		const typename locking_order_graph::vertex_descriptor *found_vertex = 0;
		typename locking_order_graph::vertex_descriptor new_vertex;
		typename vertex_finder_type::scoped_lock finder;
		if(mutex.node_key())
		{
			found_vertex = finder->find_vertex(*mutex.node_key());
		}
		if(found_vertex)
		{
			new_vertex = *found_vertex;
		}else
		{
			if(_free_vertices.size() > 0)
			{
				new_vertex = _free_vertices.back();
				_free_vertices.pop_back();
			}else
			{
				new_vertex = boost::add_vertex(_graph);
			}
			std::ostringstream node_name;
			if(mutex.node_key())
				node_name << *mutex.node_key();
			else
				node_name << "mutex " << new_vertex;
			_graph[new_vertex].name = node_name.str();
			if(mutex.node_key())
				finder->add_vertex(*mutex.node_key(), new_vertex);
		}
		mutex.set_vertex(new_vertex);
		++_graph[new_vertex].use_count;
	}

	template<typename AcyclicMutex>
	void mutex_grapher::release_vertex(const AcyclicMutex &mutex)
	{
		// we want to ignore all locking events after the first cycle is detected.
		if(_cycle_detected) return;

		if(!mutex.vertex()) return;
		const typename locking_order_graph::vertex_descriptor vertex = *mutex.vertex();

		--_graph[vertex].use_count;
		assert(_graph[vertex].use_count >= 0);
		if(_graph[vertex].use_count > 0) return;

		/* Connect all source vertices of incoming edges and to the target vertex of each outgoing edge,
		then delete all edges connected to the removed vertex. */
		std::pair<locking_order_graph::in_edge_iterator, locking_order_graph::in_edge_iterator> in_edges =
			boost::in_edges(vertex, _graph);
		locking_order_graph::in_edge_iterator in_it;
		for(in_it = in_edges.first; in_it != in_edges.second; ++in_it)
		{
			std::pair<locking_order_graph::out_edge_iterator, locking_order_graph::out_edge_iterator> out_edges =
				boost::out_edges(vertex, _graph);
			locking_order_graph::out_edge_iterator out_it;
			for(out_it = out_edges.first; out_it != out_edges.second; ++out_it)
			{
				const locking_order_graph::vertex_descriptor source = boost::source(*in_it, _graph);
				const locking_order_graph::vertex_descriptor target = boost::target(*out_it, _graph);
				assert(source != vertex);
				assert(target != vertex);
				boost::add_edge(source, target, _graph);
			}
		}
		boost::clear_vertex(vertex, _graph);

		// clear key key from finder
		if(mutex.node_key())
		{
			typedef detail::vertex_finder<typename AcyclicMutex::key_type, typename AcyclicMutex::key_compare> vertex_finder_type;
			typename vertex_finder_type::scoped_lock finder;
			finder->remove_vertex(*mutex.node_key());
		}

		/* We don't actually remove the vertex from the graph, since that would invalidate all the
		other vertex descriptors. Instead, we maintain a list of vertices available for reuse. */
		_free_vertices.push_back(vertex);
	}
};

#endif	// _POET_MUTEX_GRAPHER_IPP
