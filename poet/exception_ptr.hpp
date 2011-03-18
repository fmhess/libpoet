#ifndef POET_EXCEPTION_PTR_HPP_INCLUDED
#define POET_EXCEPTION_PTR_HPP_INCLUDED

// Copyright (c) 2007 Frank Mori Hess
// Copyright (c) 2007 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/shared_ptr.hpp>
#include <poet/exceptions.hpp>

namespace poet
{
	namespace detail
	{
		class _exp_throwable
		{
		protected:

			virtual ~_exp_throwable() {}

		public:

			virtual void rethrow() = 0;
		};

		template< class E > class _exp_throwable_impl: public _exp_throwable
		{
		private:

			E e_;

		public:

			_exp_throwable_impl()
			{
			}

			template< class A > _exp_throwable_impl( A const & a ): e_( a )
			{
			}

			void rethrow()
			{
				throw e_;
			}
		};
	}
	typedef boost::shared_ptr<detail::_exp_throwable > exception_ptr;
	inline exception_ptr current_exception();
	inline void rethrow_exception( exception_ptr p );
	template< class E >
	poet::exception_ptr copy_exception( E const & e )
	{
		exception_ptr exp(new detail::_exp_throwable_impl<E>(e));
		return exp;
	}
}

#include <poet/exception_ptr.cpp>

#endif // #ifndef POET_EXCEPTION_PTR_HPP_INCLUDED
