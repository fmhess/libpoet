/*
	Template for ActiveFunction1, ActiveFunction2, ... classes that implement
	ActiveFunctions with signatures that have 1, 2, ... parameters

	Author: Frank Hess <frank.hess@nist.gov>
	Begin: 2007-01-26
*/
/* This software was developed at the National Institute of Standards and
 * Technology by employees of the Federal Government in the course of
 * their official duties. Pursuant to title 17 Section 105 of the United
 * States Code this software is not subject to copyright protection and is
 * in the public domain. This is an experimental system. NIST assumes no
 * responsibility whatsoever for its use by other parties, and makes no
 * guarantees, expressed or implied, about its quality, reliability, or
 * any other characteristic. We would appreciate acknowledgement if the
 * software is used.
 */

// This file is included iteratively, and should not be protected from multiple inclusion

#define POET_ACTIVE_FUNCTION_NUM_ARGS BOOST_PP_ITERATION()

#define POET_ACTIVE_FUNCTION_CLASS_NAME BOOST_PP_CAT(active_function, POET_ACTIVE_FUNCTION_NUM_ARGS)

// typename poet::future<boost::function_traits<Signature>::argn_type>
#define POET_ACTIVE_FUNCTION_ARG_TYPE(z, n, Signature) \
	poet::future< BOOST_PP_CAT(BOOST_PP_CAT(typename boost::function_traits<Signature>::arg, BOOST_PP_INC(n)), _type) >
// nameStem(n + 1)
#define POET_ACTIVE_FUNCTION_ARG_NAME(z, n, nameStem) BOOST_PP_CAT(nameStem, BOOST_PP_INC(n))
// poet::future<typename boost::function_traits<Signature>::argn_type> argn
#define POET_ACTIVE_FUNCTION_FULL_ARG(z, n, Signature) \
	POET_ACTIVE_FUNCTION_ARG_TYPE(~, n, Signature) POET_ACTIVE_FUNCTION_ARG_NAME(~, n, arg)
// poet::future<typename boost::function_traits<Signature>::arg1_type> arg1,
// poet::future<typename boost::function_traits<Signature>::arg2_type> arg2,
// ...
// poet::future<typename boost::function_traits<Signature>::argn_type> argn
#define POET_ACTIVE_FUNCTION_FULL_ARGS(arity, Signature) \
	BOOST_PP_ENUM(arity, POET_ACTIVE_FUNCTION_FULL_ARG, Signature)
// nameStem1, nameStem2, ... , nameStemn
#define POET_ACTIVE_FUNCTION_ARG_NAMES(arity, nameStem) \
	BOOST_PP_ENUM(arity, POET_ACTIVE_FUNCTION_ARG_NAME, nameStem)
// typename poet::future<boost::function_traits<Signature>::argn_type> _argn ;
#define POET_ACTIVE_FUNCTION_ARG_DECLARATION(z, n, Signature) POET_ACTIVE_FUNCTION_ARG_TYPE(~, n, Signature) \
	POET_ACTIVE_FUNCTION_ARG_NAME(~, n, _arg) ;
// _argn ( argn )
#define POET_ACTIVE_FUNCTION_ARG_CONSTRUCTOR(z, n, data) \
	POET_ACTIVE_FUNCTION_ARG_NAME(~, n, _arg) ( POET_ACTIVE_FUNCTION_ARG_NAME(~, n, arg) )
// tupleName.get < n >()
#define POET_ACTIVE_FUNCTION_GET_TUPLE_ELEMENT(z, n, tupleName) \
	tupleName.get< n >()

namespace poet
{
	namespace detail
	{
		template<typename Signature>
		class POET_ACTIVE_FUNCTION_CLASS_NAME
		{
		public:
			typedef typename boost::function_traits<Signature>::result_type bare_result_type;
			typedef future<bare_result_type> result_type;

			POET_ACTIVE_FUNCTION_CLASS_NAME(const boost::function<Signature> &passiveFunction,
				const boost::function<bool ()> &guard,
				boost::shared_ptr<scheduler_base> scheduler_in,
				boost::shared_ptr<void> servant):
				_passiveFunction(passiveFunction), _guard(guard), _scheduler(scheduler_in),
				_servant(servant), _haveServantPointer(servant)
			{
				if(_scheduler == 0) _scheduler.reset(new scheduler);
			}
			virtual ~POET_ACTIVE_FUNCTION_CLASS_NAME() {}
			result_type operator ()(POET_ACTIVE_FUNCTION_FULL_ARGS(POET_ACTIVE_FUNCTION_NUM_ARGS, Signature))
			{
				boost::shared_ptr<void> sharedServant;
				if(_haveServantPointer)
				{
					sharedServant = _servant.lock();
					if(sharedServant == 0)
					{
						throw std::runtime_error("Servant no longer exists.");
					}
				}
				result_type returnValue;
				boost::shared_ptr<active_function_method_request> methodRequest = active_function_method_request::create(
					returnValue, POET_ACTIVE_FUNCTION_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, arg) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
					_passiveFunction, _guard, sharedServant);
				_scheduler->post_method_request(methodRequest);
				return returnValue;
			}
			result_type operator ()(POET_ACTIVE_FUNCTION_FULL_ARGS(POET_ACTIVE_FUNCTION_NUM_ARGS, Signature)) const
			{
				boost::shared_ptr<void> sharedServant;
				if(_haveServantPointer)
				{
					sharedServant = _servant.lock();
					if(sharedServant == 0)
					{
						throw std::runtime_error("Servant no longer exists.");
					}
				}
				result_type returnValue;
				boost::shared_ptr<active_function_method_request> methodRequest = active_function_method_request::create(
					returnValue, POET_ACTIVE_FUNCTION_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, arg) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
					_passiveFunction, _guard, sharedServant);
				_scheduler->post_method_request(methodRequest);
				return returnValue;
			}
			void wake() {_scheduler->wake();}
		private:
			class active_function_method_request: public method_request<typename POET_ACTIVE_FUNCTION_CLASS_NAME::bare_result_type>
			{
			public:
				typedef method_request<typename POET_ACTIVE_FUNCTION_CLASS_NAME::bare_result_type> base_type;
				// static factory method
				static boost::shared_ptr<active_function_method_request> create(POET_ACTIVE_FUNCTION_CLASS_NAME::result_type returnValue,
					POET_ACTIVE_FUNCTION_FULL_ARGS(POET_ACTIVE_FUNCTION_NUM_ARGS, Signature) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
					const boost::function<Signature> &passiveFunction, const boost::function<bool ()> &guard,
					boost::shared_ptr<void> servant)
				{
					return deconstruct_ptr(new active_function_method_request(returnValue, POET_ACTIVE_FUNCTION_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, arg)
						BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS) passiveFunction, guard, servant));
				}
				virtual ~active_function_method_request()
				{
				}
				virtual void run()
				{
					this->_returnValue = _passiveFunction(
						POET_ACTIVE_FUNCTION_ARG_NAMES(POET_ACTIVE_FUNCTION_NUM_ARGS, _arg)
						);
				}
				virtual bool ready() const
				{
					/* We return ready when all the future arguments are ready, and the guard is true. */
// if(_argn.ready() == false) return false;
#define POET_ACTIVE_FUNCTION_MISC_STATEMENT(z, n, nameStem) \
	if(POET_ACTIVE_FUNCTION_ARG_NAME(~, n, nameStem).ready() == false) return false;
					BOOST_PP_REPEAT(POET_ACTIVE_FUNCTION_NUM_ARGS, POET_ACTIVE_FUNCTION_MISC_STATEMENT, _arg)
#undef POET_ACTIVE_FUNCTION_MISC_STATEMENT
					if(_guard) return _guard();
					return true;
				}
			protected:
				active_function_method_request(typename POET_ACTIVE_FUNCTION_CLASS_NAME::result_type returnValue,
					POET_ACTIVE_FUNCTION_FULL_ARGS(POET_ACTIVE_FUNCTION_NUM_ARGS, Signature) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
					const boost::function<Signature> &passiveFunction, const boost::function<bool ()> &guard,
					boost::shared_ptr<void> servant): base_type(returnValue),
					BOOST_PP_ENUM(POET_ACTIVE_FUNCTION_NUM_ARGS, POET_ACTIVE_FUNCTION_ARG_CONSTRUCTOR, ~) BOOST_PP_COMMA_IF(POET_ACTIVE_FUNCTION_NUM_ARGS)
					_passiveFunction(passiveFunction), _guard(guard), _servant(servant)
				{
					_lastReadyChanged = ready();
				}
				virtual void postconstruct()
				{
					typedef typename boost::slot<void (void)> slot_type;
					/*
					_arg1.connectUpdate(slot_type(
						&POET_ACTIVE_FUNCTION_CLASS_NAME::active_function_method_request::futureUpdateSlot, this).track(this->shared_from_this()));
					_arg2.connectUpdate(slot_type(
						&POET_ACTIVE_FUNCTION_CLASS_NAME::active_function_method_request::futureUpdateSlot, this).track(this->shared_from_this()));
					...
					_argN.connectUpdate(slot_type(
						&POET_ACTIVE_FUNCTION_CLASS_NAME::active_function_method_request::futureUpdateSlot, this).track(this->shared_from_this()));
					*/
#define POET_ACTIVE_FUNCTION_MISC_STATEMENT(z, n, nameStem) \
	POET_ACTIVE_FUNCTION_ARG_NAME(~, n, nameStem).connectUpdate(slot_type( \
		&POET_ACTIVE_FUNCTION_CLASS_NAME::active_function_method_request::futureUpdateSlot, this).track(this->shared_from_this()));
					BOOST_PP_REPEAT(POET_ACTIVE_FUNCTION_NUM_ARGS, POET_ACTIVE_FUNCTION_MISC_STATEMENT, _arg)
#undef POET_ACTIVE_FUNCTION_MISC_STATEMENT
				}
			private:
				bool futureArgIsCancelled() const
				{
// if(_argn.cancelled()) return true;
#define POET_ACTIVE_FUNCTION_MISC_STATEMENT(z, n, nameStem) \
	if(POET_ACTIVE_FUNCTION_ARG_NAME(~, n, nameStem).cancelled()) return true;
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
						this->_updateSignal();
					}
					if(futureArgIsCancelled())
					{
						this->cancel();
						/* method_request<>::cancel() emits method_request_base::_updateSignal,
						and cancels future return value, so there is no need to do it here. */
					}
					/* We don't need to worry about cancellation through the return value here,
					as that is already handled by method_request<> base class. */
				};

				// typename poet::future<boost::function_traits<Signature>::arg1_type> _arg1;
				// typename poet::future<boost::function_traits<Signature>::arg2_type> _arg2;
				// ...
				// typename poet::future<boost::function_traits<Signature>::argN_type> _argN;
				BOOST_PP_REPEAT(POET_ACTIVE_FUNCTION_NUM_ARGS, POET_ACTIVE_FUNCTION_ARG_DECLARATION, Signature)
				boost::function<Signature> _passiveFunction;
				boost::function<bool ()> _guard;
				boost::shared_ptr<void> _servant;
				bool _lastReadyChanged;
				mutable boost::mutex _lastReadyChangedMutex;
			};
			boost::function<Signature> _passiveFunction;
			boost::function<bool ()> _guard;
			boost::shared_ptr<scheduler_base> _scheduler;
			boost::weak_ptr<void> _servant;
			const bool _haveServantPointer;
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
#undef POET_ACTIVE_FUNCTION_ARG_NAME
#undef POET_ACTIVE_FUNCTION_FULL_ARG
#undef POET_ACTIVE_FUNCTION_FULL_ARGS
#undef POET_ACTIVE_FUNCTION_ARG_NAMES
#undef POET_ACTIVE_FUNCTION_ARG_DECLARATION
#undef POET_ACTIVE_FUNCTION_ARG_CONSTRUCTOR
#undef POET_ACTIVE_FUNCTION_GET_TUPLE_ELEMENT
