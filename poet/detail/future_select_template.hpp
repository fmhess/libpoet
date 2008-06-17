/*
	Template for future_barrier and future_select overloads with
	variable numbers of "const future<T> &" arguments.

	begin: Frank Hess <frank.hess@nist.gov>  2008-05-21
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

// This file is included iteratively, and should not be protected from multiple inclusion

#include <poet/detail/preprocessor_macros.hpp>

#define POET_FUTURE_SELECT_NUM_ARGS BOOST_PP_ITERATION()
// const poet::future<T> &fn
#define POET_FUTURE_SELECT_ARG_DECL(z, n, data) \
	const poet::future<T> & POET_ARG_NAME(~, n, f)
// const future<T> &f1, const future<T> &f2, ..., const future<T> &fn
#define POET_FUTURE_SELECT_REPEATED_ARG_DECLARATIONS(arity) \
	BOOST_PP_ENUM(arity, POET_FUTURE_SELECT_ARG_DECL, ~)

namespace poet
{
	template<typename T>
	future<T> future_select(POET_FUTURE_SELECT_REPEATED_ARG_DECLARATIONS(POET_FUTURE_SELECT_NUM_ARGS))
	{
		std::vector<future<T> > inputs;
// inputs.push_back(fn);
#define POET_MISC_STATEMENT(z, n, data) \
	inputs.push_back(POET_ARG_NAME(~, n, f));
		BOOST_PP_REPEAT(POET_FUTURE_SELECT_NUM_ARGS, POET_MISC_STATEMENT, ~)
#undef POET_MISC_STATEMENT
		return future_select_range(inputs.begin(), inputs.end());
	}
}

#undef POET_FUTURE_SELECT_NUM_ARGS
#undef POET_FUTURE_SELECT_ARG_DECL
#undef POET_FUTURE_SELECT_REPEATED_ARG_DECLARATIONS
