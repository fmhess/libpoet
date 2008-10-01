/*
	A template class designed to make it easier to produce
	active objects and asynchronous objects.

	Author: Frank Hess <frank.hess@nist.gov>
	Begin: 2007-01-23
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_ACTIVE_FUNCTION_HPP
#define _POET_ACTIVE_FUNCTION_HPP

#include <boost/preprocessor/arithmetic.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/iteration.hpp>
#include <boost/preprocessor/repetition.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/signals2/slot.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/type_traits.hpp>
#include <boost/weak_ptr.hpp>
#include <poet/active_object.hpp>
#include <poet/future.hpp>
#include <poet/future_barrier.hpp>
#include <vector>

#ifndef POET_ACTIVE_FUNCTION_MAX_ARGS
#define POET_ACTIVE_FUNCTION_MAX_ARGS 10
#endif

#define BOOST_PP_ITERATION_LIMITS (0, POET_ACTIVE_FUNCTION_MAX_ARGS)
#define BOOST_PP_FILENAME_1 <poet/detail/active_function_template.hpp>
#include BOOST_PP_ITERATE()

namespace poet
{
	template<typename Signature>
	class active_function: public detail::active_functionN<boost::function_traits<Signature>::arity, Signature>::type
	{
	private:
		typedef typename detail::active_functionN<boost::function_traits<Signature>::arity, Signature>::type base_type;

	public:
		active_function()
		{}
		active_function(const typename base_type::passive_slot_type &passive_function,
			boost::shared_ptr<scheduler_base> scheduler = boost::shared_ptr<scheduler_base>()):
			base_type(passive_function, scheduler)
		{}
		virtual ~active_function() {}
	};
}

#endif	// _POET_ACTIVE_FUNCTION_HPP
