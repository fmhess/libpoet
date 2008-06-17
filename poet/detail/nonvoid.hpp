/*
	Maps void type to bogus int, leaves all other types the same.
	Added to ease generic programming while dealing with future<void> issues.

	begin: Frank Hess <frank.hess@nist.gov>  2008-05-28
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_NONVOID_HPP
#define _POET_NONVOID_HPP

#include <boost/mpl/if.hpp>
#include <boost/type_traits.hpp>
#include <poet/null_type.hpp>

namespace poet
{
	namespace detail
	{

		template<typename T>
			class nonvoid
		{
		public:
			typedef typename boost::mpl::if_<boost::is_void<T>, null_type, T>::type type;
		};
	}
}

#endif //_POET_NONVOID_HPP
