/*
	begin: Frank Hess <frank.hess@nist.gov>  2008-06-08
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_IDENTITY_HPP
#define _POET_IDENTITY_HPP

namespace poet
{
	namespace detail
	{
		struct identity
		{
			template<typename T>
			const T& operator()(const T &t) const
			{
				return t;
			}
			template<typename T>
			T& operator()(T &t) const
			{
				return t;
			}
		};
	}
}

#endif //_POET_IDENTITY_HPP
