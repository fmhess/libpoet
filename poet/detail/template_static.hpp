/*
	A hack to add static members to non-template classes without
	needing a compiled lib to hold definition.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-10-26
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_TEMPLATE_STATIC_HPP
#define _POET_TEMPLATE_STATIC_HPP

namespace poet
{
	namespace detail
	{
		template<typename AssociatedClass, typename T> class template_static
		{
		public:
			static T object;
		};
		template<typename AssociatedClass, typename T>
		T template_static<AssociatedClass, T>::object;
	};
};

#endif // _POET_STATIC_MUTEX_HPP
