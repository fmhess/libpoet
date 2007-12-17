/*
	A mutex wrapper which automatically tracks locking order to insure no deadlocks
	are possible.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-10-26
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_ACYCLIC_MUTEX_BASE_HPP
#define _POET_ACYCLIC_MUTEX_BASE_HPP

#include <boost/optional.hpp>
#include <boost/thread/mutex.hpp>
#include <poet/detail/template_static.hpp>
#include <poet/detail/mutex_grapher_decl.hpp>
#include <sstream>
#include <string>

#ifdef NDEBUG
#ifndef ACYCLIC_MUTEX_NDEBUG
#define ACYCLIC_MUTEX_NDEBUG
#endif	// ACYCLIC_MUTEX_NDEBUG
#endif	// NDEBUG

namespace poet
{
#ifdef ACYCLIC_MUTEX_NDEBUG
	class acyclic_mutex_base
	{
	protected:
		typedef mutex_grapher::locking_order_graph::vertex_descriptor vertex_descriptor_type;
	public:
		virtual ~acyclic_mutex_base() {}

		boost::optional<vertex_descriptor_type> vertex() const {return boost::optional<vertex_descriptor_type>();}
	protected:
		friend class poet::mutex_grapher;

		void set_vertex(vertex_descriptor_type vertex)
		{}
		virtual bool will_really_lock() const
		{
			return true;
		}
		virtual bool will_really_unlock() const
		{
			return true;
		}
		virtual void increment_recursive_lock_count()
		{}
		virtual void decrement_recursive_lock_count()
		{}
	};
#else	// ACYCLIC_MUTEX_NDEBUG not defined

	class acyclic_mutex_base
	{
	protected:
		typedef mutex_grapher::locking_order_graph::vertex_descriptor vertex_descriptor_type;
	public:
		virtual ~acyclic_mutex_base() {}

		boost::optional<vertex_descriptor_type> vertex() const {return _vertex;}
	protected:
		friend class poet::mutex_grapher;

		void set_vertex(vertex_descriptor_type vertex)
		{
			_vertex = vertex;
		}
		virtual bool will_really_lock() const
		{
			return true;
		}
		virtual bool will_really_unlock() const
		{
			return true;
		}
		virtual void increment_recursive_lock_count()
		{}
		virtual void decrement_recursive_lock_count()
		{}

		boost::optional<vertex_descriptor_type> _vertex;
	};
#endif	// ACYCLIC_MUTEX_NDEBUG
};

#endif // _POET_ACYCLIC_MUTEX_BASE_HPP
