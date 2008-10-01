/*
	Template for ActiveFunction1, ActiveFunction2, ... classes that implement
	ActiveFunctions with signatures that have 1, 2, ... parameters

	Author: Frank Hess <frank.hess@nist.gov>
	Begin: 2007-01-26
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

// This file is included iteratively, and should not be protected from multiple inclusion

#include <poet/detail/preprocessor_macros.hpp>

#define POET_ACTIVE_FUNCTION_NUM_ARGS BOOST_PP_ITERATION()

#define POET_ACTIVE_FUNCTION_CLASS_NAME BOOST_PP_CAT(active_function, POET_ACTIVE_FUNCTION_NUM_ARGS)
#define POET_AF_METHOD_REQUEST_CLASS_NAME BOOST_PP_CAT(active_function_method_request, POET_ACTIVE_FUNCTION_NUM_ARGS)

// typename poet::future<boost::function_traits<Signature>::argn_type>
#define POET_ACTIVE_FUNCTION_ARG_TYPE(z, n, Signature) \
	poet::future< BOOST_PP_CAT(BOOST_PP_CAT(typename boost::function_traits<Signature>::arg, BOOST_PP_INC(n)), _type) >
// poet::future<typename boost::function_traits<Signature>::argn_type> argn
#define POET_ACTIVE_FUNCTION_FULL_ARG(z, n, Signature) \
	POET_ACTIVE_FUNCTION_ARG_TYPE(~, n, Signature) POET_ARG_NAME(~, n, arg)
// poet::future<typename boost::function_traits<Signature>::arg1_type> arg1,
// poet::future<typename boost::function_traits<Signature>::arg2_type> arg2,
// ...
// poet::future<typename boost::function_traits<Signature>::argn_type> argn
#define POET_ACTIVE_FUNCTION_FULL_ARGS(arity, Signature) \
	BOOST_PP_ENUM(arity, POET_ACTIVE_FUNCTION_FULL_ARG, Signature)
// typename poet::future<boost::function_traits<Signature>::argn_type> _argn ;
#define POET_ACTIVE_FUNCTION_ARG_DECLARATION(z, n, Signature) POET_ACTIVE_FUNCTION_ARG_TYPE(~, n, Signature) \
	POET_ARG_NAME(~, n, _arg) ;
// tupleName.get < n >()
#define POET_ACTIVE_FUNCTION_GET_TUPLE_ELEMENT(z, n, tupleName) \
	tupleName.get< n >()

namespace poet
{
	namespace detail
	{
		template <typename Signature>
		class POET_AF_METHOD_REQUEST_CLASS_NAME:
			public method_request_base
		{
		public:
			typedef typename boost::function_traits<Signature>::result_type passive_result_type;
			typedef method_request_base base_type;
			typedef typename boost::signals2::slot<Signature> passive_slot_type;

			POET_AF_METHOD_REQUEST_CLASS_NAME(const promise<passive_result_type> &returnValue,
				POET_ACTIVE_FUNCTION_FULL_ARGS(POET_ACTIVE_FUNCTION_NUM_ARGS, Signature) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
				const boost::shared_ptr<boost::signals2::slot<Signature> > &passive_function): _return_value(returnValue),
				POET_REPEATED_ARG_CONSTRUCTOR(POET_ACTIVE_FUNCTION_NUM_ARGS, arg) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
				_passive_function(passive_function)
			{}
			virtual ~POET_AF_METHOD_REQUEST_CLASS_NAME()
			{}

			virtual void run()
			{
				try
				{
					passive_result_type *resolver = 0;
					m_run(resolver);
				}
				catch(...)
				{
					_return_value.renege(current_exception());
				}
			}
			virtual future<void> scheduling_guard() const
			{
#if POET_ACTIVE_FUNCTION_NUM_ARGS == 0
				future<int> ready_future = 1;
				return ready_future;
#elif POET_ACTIVE_FUNCTION_NUM_ARGS == 1
				return _arg1;
#else
				return future_barrier(POET_REPEATED_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, _arg));
#endif
			}
		private:
			void m_run(void *)
			{
				(*_passive_function)(
					POET_REPEATED_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, _arg));
				_return_value.fulfill();
			}
			template <typename U>
			void m_run(U *)
			{
				_return_value.fulfill((*_passive_function)(
					POET_REPEATED_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, _arg)));
			}

			promise<passive_result_type> _return_value;
			// typename poet::future<boost::function_traits<Signature>::arg1_type> _arg1;
			// typename poet::future<boost::function_traits<Signature>::arg2_type> _arg2;
			// ...
			// typename poet::future<boost::function_traits<Signature>::argN_type> _argN;
			BOOST_PP_REPEAT(POET_ACTIVE_FUNCTION_NUM_ARGS, POET_ACTIVE_FUNCTION_ARG_DECLARATION, Signature)
			boost::shared_ptr<boost::signals2::slot<Signature> > _passive_function;
		};

		template<typename Signature>
		class POET_ACTIVE_FUNCTION_CLASS_NAME
		{
		public:
			typedef typename boost::function_traits<Signature>::result_type passive_result_type;
			typedef future<passive_result_type> result_type;
			typedef boost::signals2::slot<Signature> passive_slot_type;

			POET_ACTIVE_FUNCTION_CLASS_NAME()
			{}
			POET_ACTIVE_FUNCTION_CLASS_NAME(const passive_slot_type &passive_function,
				boost::shared_ptr<scheduler_base> scheduler_in):
				_passive_function(new passive_slot_type(passive_function)),
				_scheduler(scheduler_in)
			{
				if(_scheduler == 0) _scheduler.reset(new scheduler);
			}
			virtual ~POET_ACTIVE_FUNCTION_CLASS_NAME() {}
			result_type operator ()(POET_ACTIVE_FUNCTION_FULL_ARGS(POET_ACTIVE_FUNCTION_NUM_ARGS, Signature))
			{
				promise<passive_result_type> returnValue;
				boost::shared_ptr<POET_AF_METHOD_REQUEST_CLASS_NAME<Signature> > methodRequest(
					new POET_AF_METHOD_REQUEST_CLASS_NAME<Signature>(
					returnValue, POET_REPEATED_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, arg) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
					_passive_function));
				_scheduler->post_method_request(methodRequest);
				return returnValue;
			}
			result_type operator ()(POET_ACTIVE_FUNCTION_FULL_ARGS(POET_ACTIVE_FUNCTION_NUM_ARGS, Signature)) const
			{
				promise<passive_result_type> returnValue;
				boost::shared_ptr<POET_AF_METHOD_REQUEST_CLASS_NAME<Signature> > methodRequest(
					new POET_AF_METHOD_REQUEST_CLASS_NAME<Signature>(
					returnValue, POET_REPEATED_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, arg) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
					_passive_function));
				_scheduler->post_method_request(methodRequest);
				return returnValue;
			}
			bool expired() const {return _passive_function.expired();}
		private:
			boost::shared_ptr<passive_slot_type> _passive_function;
			boost::shared_ptr<scheduler_base> _scheduler;
		};

		template<unsigned arity, typename Signature> class active_functionN;
		// partial template specialization
		template<typename Signature>
		class active_functionN<POET_ACTIVE_FUNCTION_NUM_ARGS, Signature>
		{
		public:
			typedef POET_ACTIVE_FUNCTION_CLASS_NAME<Signature> type;
		};
	}
}

#undef POET_ACTIVE_FUNCTION_NUM_ARGS
#undef POET_ACTIVE_FUNCTION_CLASS_NAME
#undef POET_ACTIVE_FUNCTION_ARG_TYPE
#undef POET_ACTIVE_FUNCTION_FULL_ARG
#undef POET_ACTIVE_FUNCTION_FULL_ARGS
#undef POET_ACTIVE_FUNCTION_ARG_DECLARATION
#undef POET_ACTIVE_FUNCTION_GET_TUPLE_ELEMENT
