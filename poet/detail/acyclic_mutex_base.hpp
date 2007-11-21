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

#include <boost/thread/mutex.hpp>
#include <poet/detail/static_mutex.hpp>
#include <sstream>
#include <string>

namespace poet
{
	namespace detail
	{
		class acyclic_mutex_base
		{
		public:
			acyclic_mutex_base(const std::string &node_key): _node_key(node_key)
			{
				if(_node_key == "")
				{
					std::ostringstream key_stream;
					key_stream << generate_mutex_id();
					_node_key = key_stream.str();
				}
			}
			const std::string& node_key() const {return _node_key;}
		private:
			static uint64_t generate_mutex_id()
			{
				static uint64_t next_id = 0;

				boost::mutex::scoped_lock lock(detail::static_mutex<acyclic_mutex_base>::mutex);
				return next_id++;
			}

			std::string _node_key;
		};
	};
};

#endif // _POET_ACYCLIC_MUTEX_BASE_HPP
