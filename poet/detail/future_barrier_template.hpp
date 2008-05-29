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

#define POET_FUTURE_WAITS_NUM_ARGS BOOST_PP_ITERATION()
// const poet::future<Tn> &
#define POET_FUTURE_WAITS_ARG_TYPE(z, n, data) \
	const poet::future<BOOST_PP_CAT(T, BOOST_PP_INC(n))> &
// const poet::future<Tn> &namestemN
#define POET_FUTURE_WAITS_ARG_DECL(z, n, namestem) \
	POET_FUTURE_WAITS_ARG_TYPE(~, n, ~) POET_ARG_NAME(~, n, namestem)
// const future<T1> &namestem1, const future<T2> &namestem2, ..., const future<Tn> &namestemN
#define POET_FUTURE_WAITS_REPEATED_ARG_DECLARATIONS(arity, namestem) \
	BOOST_PP_ENUM(arity, POET_FUTURE_WAITS_ARG_DECL, namestem)
// future_hetro_combining_barrier_N
#define FUTURE_HETRO_COMBINING_BARRIER_BODY_N BOOST_PP_CAT(future_hetro_combining_barrier_, POET_FUTURE_WAITS_NUM_ARGS)
// combiner_invoker_N
#define POET_COMBINER_INVOKER_N BOOST_PP_CAT(combiner_invoker_, POET_FUTURE_WAITS_NUM_ARGS)

namespace poet
{
	namespace detail
	{
		template<typename R, typename Combiner>
		class POET_COMBINER_INVOKER_N
		{
		public:
			POET_COMBINER_INVOKER_N(const Combiner &combiner):
				_combiner(combiner)
			{}
			template<POET_REPEATED_TYPENAMES(POET_FUTURE_WAITS_NUM_ARGS, T)>
			void operator()(boost::optional<R> &result, POET_FUTURE_WAITS_REPEATED_ARG_DECLARATIONS(POET_FUTURE_WAITS_NUM_ARGS, f))
			{
				result = _combiner(
/*
nonvoid_future_get<T1>(f1),
nonvoid_future_get<T2>(f2),
...
nonvoid_future_get<TN>(fN)
*/
#define POET_MISC_STATEMENT(z, n, data) \
	nonvoid_future_get<POET_ARG_NAME(~, n, T)>( POET_ARG_NAME(~, n, f) )
					BOOST_PP_ENUM(POET_FUTURE_WAITS_NUM_ARGS, POET_MISC_STATEMENT, ~)
#undef POET_MISC_STATEMENT
					);
			}
		private:
			Combiner _combiner;
		};
		template<typename Combiner>
		class POET_COMBINER_INVOKER_N<void, Combiner>
		{
		public:
			POET_COMBINER_INVOKER_N(const Combiner &combiner):
				_combiner(combiner)
			{}
			template<POET_REPEATED_TYPENAMES(POET_FUTURE_WAITS_NUM_ARGS, T)>
			void operator()(boost::optional<nonvoid<void>::type> &result,
				POET_FUTURE_WAITS_REPEATED_ARG_DECLARATIONS(POET_FUTURE_WAITS_NUM_ARGS, f))
			{
				_combiner(
/*
nonvoid_future_get<T1>(f1),
nonvoid_future_get<T2>(f2),
...
nonvoid_future_get<TN>(fN)
*/
#define POET_MISC_STATEMENT(z, n, data) \
	nonvoid_future_get<POET_ARG_NAME(~, n, T)>( POET_ARG_NAME(~, n, f) )
					BOOST_PP_ENUM(POET_FUTURE_WAITS_NUM_ARGS, POET_MISC_STATEMENT, ~)
#undef POET_MISC_STATEMENT
					);
				result = bogus_void();
			}
		private:
			Combiner _combiner;
		};

		template<typename R, typename Combiner, POET_REPEATED_TYPENAMES(POET_FUTURE_WAITS_NUM_ARGS, T)>
		class FUTURE_HETRO_COMBINING_BARRIER_BODY_N :
			public future_body_base<R>
		{
		public:
			template<typename InputFutureIterator>
			FUTURE_HETRO_COMBINING_BARRIER_BODY_N(const Combiner &combiner,
				POET_FUTURE_WAITS_REPEATED_ARG_DECLARATIONS(POET_FUTURE_WAITS_NUM_ARGS, input_future)):
				POET_REPEATED_ARG_CONSTRUCTOR(POET_FUTURE_WAITS_NUM_ARGS, input_future),
				_combiner_invoker(combiner),
				_impl(boost::bind(&FUTURE_HETRO_COMBINING_BARRIER_BODY_N::completion_handler, this),
					boost::bind(&FUTURE_HETRO_COMBINING_BARRIER_BODY_N::invoke_combiner, this))
			{
				std::vector<future<void> > input_futures;
/*
input_futures.push_back(_input_future1);
input_futures.push_back(_input_future2);
...
input_futures.push_back(_input_futureN);
*/
#define POET_MISC_STATEMENT(z, n, data) \
	input_futures.push_back(POET_ARG_NAME(~, n, _input_future));
				BOOST_PP_REPEAT(POET_FUTURE_WAITS_NUM_ARGS, POET_MISC_STATEMENT, ~)
#undef POET_MISC_STATEMENT
				_impl.set_input_futures(input_futures.begin(), input_futures.end());
			}
			virtual bool ready() const
			{
				return _impl.ready();
			}
			virtual void join() const
			{
				return _impl.join();
			}
			virtual bool timed_join(const boost::system_time &absolute_time) const
			{
				return _impl.timed_join(absolute_time);
			}
			virtual void cancel(const poet::exception_ptr &exp)
			{}
			virtual exception_ptr get_exception_ptr() const
			{
				return _impl.get_exception_ptr();
			}
			virtual const typename nonvoid<R>::type& getValue() const
			{
				this->join();
				return *this->_combiner_result;
			}
			virtual void setValue(const typename nonvoid<R>::type &value)
			{
				BOOST_ASSERT(false);
			}
		private:
			void invoke_combiner()
			{
				_combiner_invoker(_combiner_result,
					POET_REPEATED_ARG_NAMES(POET_FUTURE_WAITS_NUM_ARGS, _input_future));
			}
			void completion_handler()
			{
				this->_updateSignal();
			}
/*
future<T1> _input_future1;
future<T2> _input_future2;
...
future<TN> _input_futureN;
*/
#define POET_MISC_STATEMENT(z, n, data) \
	future<POET_ARG_NAME(~, n, T)> POET_ARG_NAME(~, n, _input_future);
		BOOST_PP_REPEAT(POET_FUTURE_WAITS_NUM_ARGS, POET_MISC_STATEMENT, ~)
#undef POET_MISC_STATEMENT
			POET_COMBINER_INVOKER_N<R, Combiner> _combiner_invoker;
			boost::optional<typename nonvoid<R>::type> _combiner_result;
			future_barrier_body_impl _impl;
		};
	}

	template<POET_REPEATED_TYPENAMES(POET_FUTURE_WAITS_NUM_ARGS, T)>
	future<void> future_barrier(POET_FUTURE_WAITS_REPEATED_ARG_DECLARATIONS(POET_FUTURE_WAITS_NUM_ARGS, f))
	{
		std::vector<future<void> > inputs;
/*
inputs.push_back(f1);
inputs.push_back(f2);
...
inputs.push_back(fn);
*/
#define POET_MISC_STATEMENT(z, n, data) \
	inputs.push_back(POET_ARG_NAME(~, n, f));
		BOOST_PP_REPEAT(POET_FUTURE_WAITS_NUM_ARGS, POET_MISC_STATEMENT, ~)
#undef POET_MISC_STATEMENT
		return future_barrier_range(inputs.begin(), inputs.end());
	}

	template<typename R, typename Combiner, POET_REPEATED_TYPENAMES(POET_FUTURE_WAITS_NUM_ARGS, T)>
	future<R> future_combining_barrier(const Combiner &combiner,
		POET_FUTURE_WAITS_REPEATED_ARG_DECLARATIONS(POET_FUTURE_WAITS_NUM_ARGS, f))
	{
		typedef detail::FUTURE_HETRO_COMBINING_BARRIER_BODY_N<R, Combiner, POET_REPEATED_ARG_NAMES(POET_FUTURE_WAITS_NUM_ARGS, T)> body_type;
		future<R> result(boost::shared_ptr<body_type>(
			new body_type(combiner, POET_REPEATED_ARG_NAMES(POET_FUTURE_WAITS_NUM_ARGS, f))));
		return result;
	}
}

#undef POET_FUTURE_WAITS_NUM_ARGS
#undef POET_FUTURE_WAITS_ARG_TYPE
#undef POET_FUTURE_WAITS_ARG_DECL
#undef POET_FUTURE_WAITS_REPEATED_ARG_DECLARATIONS
#undef FUTURE_HETRO_COMBINING_BARRIER_BODY_N
#undef POET_COMBINER_INVOKER_N