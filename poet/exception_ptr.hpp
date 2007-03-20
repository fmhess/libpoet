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
	/*! \brief Transport an arbitrary exception.

	libpoet uses exception_ptr to transport an exception thrown by
	a passive function being executed inside a scheduler thread to
	a future waiting on its result.  It is
	adapted from Peter
	Dimov's <a href="http://www.pdimov.com/cpp/N2179.html">N2179 proposal</a>.
	*/
	typedef boost::shared_ptr<detail::_exp_throwable > exception_ptr;
	/*! \brief Get an exception_ptr corresponding to the current exception.

	current_exception() can be used inside a catch block to get a copy
	of the current exception.  This is especially useful inside catch(...)
	blocks where there is no explicit parameter corresponding to the exception.

	Due to the limitations of exception handling in C++, current_exception()
	does not work perfectly.  Only exception types specifically known by
	the implementation are captured correctly.  Exceptions derived from the
	known exceptions will be captured as the most derived base class which
	is a known exception.  Other exceptions will only be captured as
	poet::unknown_exception objects.  If current_exception() can only
	determine that the exception is derived from std::exception(), then
	the exception will also be captured as a poet::unknown_exception,
	although it will capture the correct std::exception::what() string.

	The implementation knows all the exceptions in &lt;stdexcept&gt;, as
	well as all the exception classes in libpoet and some of the
	exceptions specified in thread_safe_signals and boost.

	libpoet uses current_exception() to transport an exception thrown
	by a passive function in a method request being run in a
	scheduler's thread
	back to a thread waiting on a future corresponding to the method
	request's return value.
	*/
	inline exception_ptr current_exception();

	/*! \brief Throws the exception held by the exception_ptr. */
	inline void rethrow_exception( exception_ptr p );
	/*! \brief Creates an exception_ptr from an exception. */
	template< class E >
	poet::exception_ptr copy_exception( E const & e )
	{
		exception_ptr exp(new detail::_exp_throwable_impl<E>(e));
		return exp;
	}
}

#include <poet/exception_ptr.cpp>

#endif // #ifndef POET_EXCEPTION_PTR_HPP_INCLUDED
