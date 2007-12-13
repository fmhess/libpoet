/*
	A thread-safe singleton class which can be used build up a graph of a
	program's locking order for mutexes, and check it for locking
	order violations.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-10-26
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
#include <poet/detail/acyclic_mutex_base.hpp>
#include <poet/mutex_properties.hpp>
#include <sstream>
#include <stdint.h>
#include <string>
#include <vector>

namespace poet
{
	mutex_grapher::mutex_grapher(): _cycle_handler(&default_cycle_handler)
	{}

	template<typename AcyclicMutex>
	void mutex_grapher::track_lock(AcyclicMutex &mutex)
	{
		if(mutex.vertex_descriptor() == 0)
		{
			typedef detail::vertex_finder<typename AcyclicMutex::key_type, typename AcyclicMutex::key_compare> vertex_finder_type;

			const typename locking_order_graph::vertex_descriptor *found_vertex = 0;
			typename locking_order_graph::vertex_descriptor target_vertex;
			typename vertex_finder_type::scoped_lock finder;
			if(mutex.node_key())
			{
				found_vertex = finder->find_vertex(*mutex.node_key());
			}
			if(found_vertex)
			{
				target_vertex = *found_vertex;
			}else
			{
				target_vertex = boost::add_vertex(_graph);
				std::ostringstream node_name;
				if(mutex.node_key())
					node_name << *mutex.node_key();
				else
					node_name << "vertex " << target_vertex;
				_graph[target_vertex].name = node_name.str();
				if(mutex.node_key())
					finder->add_vertex(*mutex.node_key(), target_vertex);
			}
			mutex.set_vertex_descriptor(target_vertex);
		}
		if(mutex.will_really_lock())
		{
			bool acyclic = true;
			if(locked_mutexes().empty() == false)
			{
				const typename locking_order_graph::vertex_descriptor source_vertex = *locked_mutexes().back()->vertex_descriptor();
				typename locking_order_graph::edge_descriptor new_edge = boost::add_edge(source_vertex,
					*mutex.vertex_descriptor(), _graph).first;
				try
				{
					check_for_cycles();
				}
				catch(const boost::not_a_dag &error)
				{
					acyclic = false;
					_graph[new_edge].locking_order_violation = true;
				}
			}
			locked_mutexes().push_back(&mutex);
			if(acyclic == false)
			{
				_cycle_handler();
			}
		}
	};

	void mutex_grapher::track_unlock(const detail::acyclic_mutex_base &mutex)
	{
		if(mutex.will_really_unlock())
		{
			mutex_list_type::reverse_iterator rit = std::find(locked_mutexes().rbegin(), locked_mutexes().rend(), &mutex);
			assert(rit != locked_mutexes().rend());
			locked_mutexes().erase(--(rit.base()));
		}
	}

	void mutex_grapher::write_graphviz(std::ostream &out_stream)
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
		std::cerr << __PRETTY_FUNCTION__ << ": cycle detected in mutex locking order." << std::endl;
		std::abort();
	}
};

#endif	// _POET_MUTEX_GRAPHER_IPP
