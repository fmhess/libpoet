#ifndef EXCEPTION_PTR_HPP_INCLUDED
#define EXCEPTION_PTR_HPP_INCLUDED

// Copyright (c) 2007 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#include <boost/shared_ptr.hpp>

class _exp_throwable;

typedef boost::shared_ptr< _exp_throwable > exception_ptr;

exception_ptr current_exception();
void rethrow_exception( exception_ptr p );

template< class E > exception_ptr copy_exception( E const & e )
{
    try
    {
        throw e;
    }
    catch( ... )
    {
        return current_exception();
    }
}

#endif // #ifndef EXCEPTION_PTR_HPP_INCLUDED
