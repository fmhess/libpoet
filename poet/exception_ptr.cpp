// Copyright (c) 2007 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <poet/exception_ptr.hpp>
#include <exception>
#include <stdexcept>
// for libpoet exceptions
#include <poet/exceptions.hpp>
// for bad_weak_ptr
#include <boost/weak_ptr.hpp>
// for no_slots_error
#include <boost/last_value.hpp>
// for expired_slot
#include <boost/signals/slot.hpp>

#define _CATCH_AND_RETURN( E ) catch( E const & e ) { return poet::exception_ptr( new poet::detail::_exp_throwable_impl< E >( e ) ); }
#define _CATCH_AND_RETURN_WHAT( E ) catch( E const & e ) { return poet::exception_ptr( new poet::detail::_exp_throwable_impl< E >( e.what() ) ); }

static poet::exception_ptr _exp_current_exception()
{
	try
	{
		throw;
	}

	_CATCH_AND_RETURN_WHAT( std::invalid_argument )
	_CATCH_AND_RETURN_WHAT( std::out_of_range )
	_CATCH_AND_RETURN_WHAT( std::domain_error )
	_CATCH_AND_RETURN_WHAT( std::length_error )
	_CATCH_AND_RETURN_WHAT( std::logic_error )

	_CATCH_AND_RETURN_WHAT( poet::unknown_exception )
	_CATCH_AND_RETURN( poet::cancelled_future )
	_CATCH_AND_RETURN( poet::uncertain_future )
	_CATCH_AND_RETURN_WHAT( std::overflow_error )
	_CATCH_AND_RETURN_WHAT( std::underflow_error )
	_CATCH_AND_RETURN_WHAT( std::range_error )
	_CATCH_AND_RETURN_WHAT( std::runtime_error )

	_CATCH_AND_RETURN( boost::expired_slot )
	_CATCH_AND_RETURN( boost::bad_weak_ptr )

	_CATCH_AND_RETURN( boost::no_slots_error )
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
