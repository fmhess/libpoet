// Copyright (c) 2007 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <poet/exception_ptr.hpp>
#include <exception>
#include <stdexcept>

#define _CATCH_AND_RETURN( E ) catch( E const & e ) { return poet::exception_ptr( new poet::detail::_exp_throwable_impl< E >( e ) ); }
#define _CATCH_AND_RETURN_STDEXCEPT( E ) catch( E const & e ) { return poet::exception_ptr( new poet::detail::_exp_throwable_impl< E >( e.what() ) ); }

static poet::exception_ptr _exp_current_exception()
{
	try
	{
		throw;
	}

	_CATCH_AND_RETURN_STDEXCEPT( std::invalid_argument )
	_CATCH_AND_RETURN_STDEXCEPT( std::out_of_range )
	_CATCH_AND_RETURN_STDEXCEPT( std::domain_error )
	_CATCH_AND_RETURN_STDEXCEPT( std::length_error )
	_CATCH_AND_RETURN_STDEXCEPT( std::logic_error )

	_CATCH_AND_RETURN_STDEXCEPT( std::overflow_error )
	_CATCH_AND_RETURN_STDEXCEPT( std::underflow_error )
	_CATCH_AND_RETURN_STDEXCEPT( std::range_error )
	_CATCH_AND_RETURN_STDEXCEPT( std::runtime_error )

	_CATCH_AND_RETURN( std::bad_alloc )
	_CATCH_AND_RETURN( std::bad_cast )
	_CATCH_AND_RETURN( std::bad_typeid )
	_CATCH_AND_RETURN( std::bad_exception )

//FIXME add support for libpoet's exception classes, and from thread_safe_signals, and some from boost (bad_weak_ptr, expired_slot)

	// throw std::exception as poet::unknown_exception, since we can't initialize what() string using a std::exception
	catch( std::exception const & e )
	{
		return poet::exception_ptr( new poet::detail::_exp_throwable_impl<poet::unknown_exception>( e.what() ) );
	}
	catch( ... )
	{
		return poet::exception_ptr( new poet::detail::_exp_throwable_impl<poet::unknown_exception>() );
	}
}

static poet::exception_ptr s_bad_alloc( new poet::detail::_exp_throwable_impl< std::bad_alloc > );

poet::exception_ptr poet::current_exception()
{
    try
    {
        return _exp_current_exception();
    }
    catch( std::bad_alloc const & )
    {
        return s_bad_alloc;
    }
}

void poet::rethrow_exception( exception_ptr p )
{
    p->rethrow();
}
