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
	namespace detail
	{
#ifdef ACYCLIC_MUTEX_NDEBUG
		class acyclic_mutex_base
		{
		protected:
			typedef mutex_grapher::locking_order_graph::vertex_descriptor vertex_descriptor_type;
		public:
			const vertex_descriptor_type* vertex_descriptor() const {return 0;}
			void set_vertex_descriptor(vertex_descriptor_type vertex)
			{}
		};

		template<typename Key>
		class acyclic_mutex_keyed_base: public acyclic_mutex_base
		{
			typedef mutex_grapher::locking_order_graph::vertex_descriptor vertex_descriptor_type;
		public:
			typedef Key key_type;

			acyclic_mutex_keyed_base()
			{}
			acyclic_mutex_keyed_base(const Key &node_key)
			{}
			const Key* node_key() const {return 0;}
		};

#else	// ACYCLIC_MUTEX_NDEBUG not defined

		class acyclic_mutex_base
		{
		protected:
			typedef mutex_grapher::locking_order_graph::vertex_descriptor vertex_descriptor_type;
		public:
			const vertex_descriptor_type* vertex_descriptor() const {return _vertex_descriptor.get_ptr();}
			void set_vertex_descriptor(vertex_descriptor_type vertex)
			{
				_vertex_descriptor = vertex;
			}
		protected:
			boost::optional<vertex_descriptor_type> _vertex_descriptor;
		};

		template<typename Key>
		class acyclic_mutex_keyed_base: public acyclic_mutex_base
		{
			typedef mutex_grapher::locking_order_graph::vertex_descriptor vertex_descriptor_type;
		public:
			typedef Key key_type;

			acyclic_mutex_keyed_base()
			{}
			acyclic_mutex_keyed_base(const Key &node_key): _node_key(node_key)
			{}
			const Key* node_key() const {return _node_key.get_ptr();}
		private:

			boost::optional<Key> _node_key;
		};
#endif	// ACYCLIC_MUTEX_NDEBUG
	};
};

#endif // _POET_ACYCLIC_MUTEX_BASE_HPP
