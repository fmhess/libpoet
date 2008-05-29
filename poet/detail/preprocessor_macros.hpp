/*
	Some macros for feeding into the boost preprocessor for
	repetitive code generation.

	Author: Frank Hess <frank.hess@nist.gov>
	Begin: 2007-01-26
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_PREPROCESSOR_MACROS_HPP
#define _POET_PREPROCESSOR_MACROS_HPP

// nameStem(n + 1)
#define POET_ARG_NAME(z, n, nameStem) BOOST_PP_CAT(nameStem, BOOST_PP_INC(n))
// nameStem1, nameStem2, ... , nameStemn
#define POET_REPEATED_ARG_NAMES(arity, nameStem) \
	BOOST_PP_ENUM(arity, POET_ARG_NAME, nameStem)
// typename prefix1, typename prefix2, ..., typename prefixN
#define POET_REPEATED_TYPENAMES(arity, prefix) \
	BOOST_PP_ENUM_SHIFTED_PARAMS(BOOST_PP_INC(arity), typename prefix)
// type_prefixN argN
#define POET_ARG_DECLARATION(z, n, type_prefix) \
	BOOST_PP_CAT(type_prefix, BOOST_PP_INC(n)) BOOST_PP_CAT(arg, BOOST_PP_INC(n))
// type_prefix1 arg1, type_prefix2 arg2, ..., type_prefixn argn
#define POET_REPEATED_ARG_DECLARATIONS(arity, type_prefix) \
	BOOST_PP_ENUM(arity, POET_ARG_DECLARATION, type_prefix)
// _nameN ( nameN )
#define POET_ARG_CONSTRUCTOR(z, n, name) \
	POET_ARG_NAME(~, n, BOOST_PP_CAT(_, name)) ( POET_ARG_NAME(~, n, name) )
// _name1 ( name1 ), _name2 ( name2), ..., _nameN ( nameN )
#define POET_REPEATED_ARG_CONSTRUCTOR(arity, name) \
	BOOST_PP_ENUM(arity, POET_ARG_CONSTRUCTOR, name)

#endif	// _POET_PREPROCESSOR_MACROS_HPP
