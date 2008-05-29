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
			public method_request<typename  boost::function_traits<Signature>::result_type>
		{
		public:
			typedef typename boost::function_traits<Signature>::result_type passive_result_type;
			typedef method_request<passive_result_type> base_type;
			typedef typename boost::slot<Signature> passive_slot_type;
			typedef typename boost::slot<bool ()> guard_slot_type;

			// static factory method
			static boost::shared_ptr<POET_AF_METHOD_REQUEST_CLASS_NAME<Signature> > create(
				promise<passive_result_type> returnValue,
				POET_ACTIVE_FUNCTION_FULL_ARGS(POET_ACTIVE_FUNCTION_NUM_ARGS, Signature) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
				const boost::shared_ptr<passive_slot_type> &passive_function,
				const boost::shared_ptr<guard_slot_type> &guard = boost::shared_ptr<guard_slot_type>())
			{
				return boost::deconstruct_ptr(new POET_AF_METHOD_REQUEST_CLASS_NAME<Signature>(
					returnValue, POET_REPEATED_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, arg)
					BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS) passive_function, guard));
			}
			virtual ~POET_AF_METHOD_REQUEST_CLASS_NAME()
			{
			}
			virtual void run()
			{
				try
				{
					passive_result_type *resolver = 0;
					m_run(resolver);
				}
				catch(...)
				{
					this->return_value.renege(current_exception());
				}
			}
			virtual bool ready() const
			{
				/* We return ready if any of the future inputs has an exception. */
				// if(_argn.has_exception() == true) return true;
#define POET_ACTIVE_FUNCTION_MISC_STATEMENT(z, n, nameStem) \
	if(POET_ARG_NAME(~, n, nameStem).has_exception() == true) return true;
				BOOST_PP_REPEAT(POET_ACTIVE_FUNCTION_NUM_ARGS, POET_ACTIVE_FUNCTION_MISC_STATEMENT, _arg)
#undef POET_ACTIVE_FUNCTION_MISC_STATEMENT
				/* We also return ready when all the future inputs are ready, and the guard is true. */
// if(_argn.ready() == false) return false;
#define POET_ACTIVE_FUNCTION_MISC_STATEMENT(z, n, nameStem) \
	if(POET_ARG_NAME(~, n, nameStem).ready() == false) return false;
// if(_arg1.ready() == false) return false;
// if(_arg2.ready() == false) return false;
// ...
// if(_argN.ready() == false) return false;
				BOOST_PP_REPEAT(POET_ACTIVE_FUNCTION_NUM_ARGS, POET_ACTIVE_FUNCTION_MISC_STATEMENT, _arg)
#undef POET_ACTIVE_FUNCTION_MISC_STATEMENT
				if(_guard) return (*_guard)();
				return true;
			}
		protected:
			POET_AF_METHOD_REQUEST_CLASS_NAME(const promise<passive_result_type> &returnValue,
				POET_ACTIVE_FUNCTION_FULL_ARGS(POET_ACTIVE_FUNCTION_NUM_ARGS, Signature) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
				const boost::shared_ptr<boost::slot<Signature> > &passive_function,
				const boost::shared_ptr<boost::slot<bool ()> > &guard): base_type(returnValue),
				POET_REPEATED_ARG_CONSTRUCTOR(POET_ACTIVE_FUNCTION_NUM_ARGS, POET_ARG_CONSTRUCTOR, arg) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
				_passive_function(passive_function), _guard(guard)
			{
				_lastReadyChanged = ready();
			}
			virtual void postconstruct()
			{
				base_type::postconstruct();
				typedef typename boost::slot<void (void)> slot_type;
				/*
				_arg1.connectUpdate(slot_type(
					&POET_AF_METHOD_REQUEST_CLASS_NAME<Signature>::futureUpdateSlot, this).track(this->shared_from_this()));
				_arg2.connectUpdate(slot_type(
					&POET_AF_METHOD_REQUEST_CLASS_NAME<Signature>::futureUpdateSlot, this).track(this->shared_from_this()));
				...
				_argN.connectUpdate(slot_type(
					&POET_AF_METHOD_REQUEST_CLASS_NAME<Signature>::futureUpdateSlot, this).track(this->shared_from_this()));
				*/
#define POET_ACTIVE_FUNCTION_MISC_STATEMENT(z, n, nameStem) \
	POET_ARG_NAME(~, n, nameStem).connect_update(slot_type( \
		&POET_AF_METHOD_REQUEST_CLASS_NAME<Signature>::futureUpdateSlot, this).track(this->shared_from_this()));
				BOOST_PP_REPEAT(POET_ACTIVE_FUNCTION_NUM_ARGS, POET_ACTIVE_FUNCTION_MISC_STATEMENT, _arg)
#undef POET_ACTIVE_FUNCTION_MISC_STATEMENT
			}
		private:
			bool futureArgIsCancelled() const
			{
// if(_argn.cancelled()) return true;
#define POET_ACTIVE_FUNCTION_MISC_STATEMENT(z, n, nameStem) \
	if(POET_ARG_NAME(~, n, nameStem).has_exception()) return true;
// if(_arg1.cancelled()) return true;
// if(_arg2.cancelled()) return true;
// ...
// if(_argN.cancelled()) return true;
				BOOST_PP_REPEAT(POET_ACTIVE_FUNCTION_NUM_ARGS, POET_ACTIVE_FUNCTION_MISC_STATEMENT, _arg)
#undef POET_ACTIVE_FUNCTION_MISC_STATEMENT
				return false;
			}
			void futureUpdateSlot()
			{
				boost::mutex::scoped_lock lock(_lastReadyChangedMutex);
				bool newReady = ready();
				if(newReady != _lastReadyChanged)
				{
					_lastReadyChanged = newReady;
					this->update_signal();
				}
				if(futureArgIsCancelled())
				{
					this->cancel();
					/* method_request<>::cancel() emits method_request_base::update_signal,
					and cancels future return value, so there is no need to do it here. */
				}
				/* We don't need to worry about cancellation through the return value here,
				as that is already handled by method_request<> base class. */
			};
			void m_run(void *)
			{
				(*_passive_function)(
					POET_REPEATED_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, _arg));
				this->return_value.fulfill();
			}
			template <typename U>
			void m_run(U *)
			{
				this->return_value.fulfill((*_passive_function)(
					POET_REPEATED_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, _arg)));
			}

			// typename poet::future<boost::function_traits<Signature>::arg1_type> _arg1;
			// typename poet::future<boost::function_traits<Signature>::arg2_type> _arg2;
			// ...
			// typename poet::future<boost::function_traits<Signature>::argN_type> _argN;
			BOOST_PP_REPEAT(POET_ACTIVE_FUNCTION_NUM_ARGS, POET_ACTIVE_FUNCTION_ARG_DECLARATION, Signature)
			boost::shared_ptr<boost::slot<Signature> > _passive_function;
			boost::shared_ptr<boost::slot<bool ()> > _guard;
			bool _lastReadyChanged;
			mutable boost::mutex _lastReadyChangedMutex;
		};

		template<typename Signature>
		class POET_ACTIVE_FUNCTION_CLASS_NAME
		{
		public:
			typedef typename boost::function_traits<Signature>::result_type passive_result_type;
			typedef future<passive_result_type> result_type;
			typedef boost::slot<Signature> passive_slot_type;
			typedef boost::slot<bool ()> guard_slot_type;

			POET_ACTIVE_FUNCTION_CLASS_NAME(const passive_slot_type &passive_function,
				const guard_slot_type &guard,
				boost::shared_ptr<scheduler_base> scheduler_in):
				_passive_function(new passive_slot_type(passive_function)),
				_scheduler(scheduler_in)
			{
				if(_scheduler == 0) _scheduler.reset(new scheduler);
				if(guard.slot_function()) _guard.reset(new guard_slot_type(guard));
			}
			virtual ~POET_ACTIVE_FUNCTION_CLASS_NAME() {}
			result_type operator ()(POET_ACTIVE_FUNCTION_FULL_ARGS(POET_ACTIVE_FUNCTION_NUM_ARGS, Signature))
			{
				promise<passive_result_type> returnValue;
				boost::shared_ptr<POET_AF_METHOD_REQUEST_CLASS_NAME<Signature> > methodRequest =
					POET_AF_METHOD_REQUEST_CLASS_NAME<Signature>::create(
					returnValue, POET_REPEATED_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, arg) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
					_passive_function, _guard);
				_scheduler->post_method_request(methodRequest);
				return returnValue;
			}
			result_type operator ()(POET_ACTIVE_FUNCTION_FULL_ARGS(POET_ACTIVE_FUNCTION_NUM_ARGS, Signature)) const
			{
				promise<passive_result_type> returnValue;
				boost::shared_ptr<POET_AF_METHOD_REQUEST_CLASS_NAME<Signature> > methodRequest =
					POET_AF_METHOD_REQUEST_CLASS_NAME<Signature>::create(
					returnValue, POET_REPEATED_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, arg) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
					_passive_function, _guard);
				_scheduler->post_method_request(methodRequest);
				return returnValue;
			}
			void wake() {_scheduler->wake();}
			bool expired() const {return _passive_function.expired();}
		private:
			boost::shared_ptr<passive_slot_type> _passive_function;
			boost::shared_ptr<guard_slot_type> _guard;
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
