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
#include <poet/acyclic_mutex_base.hpp>
#include <poet/mutex_properties.hpp>
#include <sstream>
#include <stdint.h>
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
					node_name << "mutex " << target_vertex;
				_graph[target_vertex].name = node_name.str();
				if(mutex.node_key())
					finder->add_vertex(*mutex.node_key(), target_vertex);
			}
			mutex.set_vertex(target_vertex);
		}
		if(mutex.will_really_lock())
		{
			if(locked_mutexes().empty() == false)
			{
				const typename locking_order_graph::vertex_descriptor source_vertex = *locked_mutexes().back()->vertex();
				typename locking_order_graph::edge_descriptor new_edge = boost::add_edge(source_vertex,
					*mutex.vertex(), _graph).first;
				try
				{
					check_for_cycles();
				}
				catch(const boost::not_a_dag &error)
				{
					_cycle_detected = true;
					_graph[new_edge].locking_order_violation = true;
				}
			}
			internal_locked_mutexes().push_back(&mutex);
		}
		return _cycle_detected;
	};

	void mutex_grapher::track_unlock(const acyclic_mutex_base &mutex)
	{
		// we want to ignore all locking events after the first cycle is detected.
		if(_cycle_detected) return;

		if(mutex.will_really_unlock())
		{
			mutex_list_type::reverse_iterator rit = std::find(internal_locked_mutexes().rbegin(), internal_locked_mutexes().rend(), &mutex);
			assert(rit != internal_locked_mutexes().rend());
			internal_locked_mutexes().erase(--(rit.base()));
		}
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
		std::cerr << __PRETTY_FUNCTION__ << ": cycle detected in mutex locking order." << std::endl;
		std::abort();
	}
};

#endif	// _POET_MUTEX_GRAPHER_IPP
