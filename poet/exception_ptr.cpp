// Copyright (c) 2007 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include "exception_ptr.hpp"
#include <exception>
#include <stdexcept>

class _exp_throwable
{
protected:

    ~_exp_throwable() {}

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

#define _CATCH_AND_RETURN( E ) catch( E const & e ) { return exception_ptr( new _exp_throwable_impl< E >( e ) ); }

static exception_ptr _exp_current_exception()
{
    try
    {
        throw;
    }

    _CATCH_AND_RETURN( std::invalid_argument )
    _CATCH_AND_RETURN( std::out_of_range )
    // ...
    _CATCH_AND_RETURN( std::logic_error )

    _CATCH_AND_RETURN( std::bad_alloc )
    _CATCH_AND_RETURN( std::bad_cast )
    _CATCH_AND_RETURN( std::bad_typeid )
    _CATCH_AND_RETURN( std::bad_exception )

    catch( std::exception const & e )
    {
        return exception_ptr( new _exp_throwable_impl<std::runtime_error>( e.what() ) );
    }
    catch( ... )
    {
        return exception_ptr( new _exp_throwable_impl<std::bad_exception>() );
    }
}

static exception_ptr s_bad_alloc( new _exp_throwable_impl< std::bad_alloc > );

exception_ptr current_exception()
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

void rethrow_exception( exception_ptr p )
{
    p->rethrow();
}
