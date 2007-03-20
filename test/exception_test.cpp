// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/assert.hpp>
#include <iostream>
#include <poet/active_function.hpp>
#include <stdexcept>

void throw_runtime_error()
{
	std::cerr << __FUNCTION__ << std::endl;
	throw std::runtime_error("this std::runtime_error exception was transported to a future.");
}

int throw_logic_error()
{
	std::cerr << __FUNCTION__ << std::endl;
	throw std::logic_error("this std::logic_error exception was transported to a future.");
	return 0;
}

int throw_out_of_range()
{
	std::cerr << __FUNCTION__ << std::endl;
	throw std::out_of_range("this std::out_of_range exception was transported to a future.");
	return 0;
}

class custom_exception: public std::exception
{
public:
	custom_exception()
	{}
	virtual ~custom_exception() throw() {}
	virtual const char * what() const throw() {return "custom_exception";}
};

void throw_unknown_exception()
{
	std::cerr << __FUNCTION__ << std::endl;
	throw custom_exception();
}

struct unknown
{
	int value;
};

void throw_really_unknown()
{
	std::cerr << __FUNCTION__ << std::endl;
	throw unknown();
}

int main()
{
	{
		poet::active_function<void ()> thrower(&throw_runtime_error);
		poet::future<void> result = thrower();
		try
		{
			result.get();
			BOOST_ASSERT(false);
		}
		catch(const std::runtime_error &err)
		{
			std::cerr << "caught exception: " << err.what() << std::endl;
		}
		catch(...)
		{
			BOOST_ASSERT(false);
		}
	}

	{
		poet::active_function<int ()> thrower(&throw_logic_error);
		poet::future<int> result = thrower();
		try
		{
			int retval = result;
			BOOST_ASSERT(false);
			std::cout << "retval is " << retval << std::endl;
		}
		catch(const std::logic_error &err)
		{
			std::cerr << "caught exception: " << err.what() << std::endl;
		}
		catch(...)
		{
			BOOST_ASSERT(false);
		}
	}

	{
		poet::active_function<int ()> thrower(&throw_out_of_range);
		poet::future<int> result = thrower();
		try
		{
			int retval = result.get();
			BOOST_ASSERT(false);
			std::cout << "retval is " << retval << std::endl;
		}
		catch(const std::out_of_range &err)
		{
			std::cerr << "caught exception: " << err.what() << std::endl;
		}
		catch(...)
		{
			BOOST_ASSERT(false);
		}
	}

	// throw an exception derived from std::exception
	{
		poet::active_function<void ()> thrower(&throw_unknown_exception);
		poet::future<void> result = thrower();
		try
		{
			static_cast<void>(result);
			BOOST_ASSERT(false);
		}
		catch(const poet::unknown_exception &err)
		{
			std::cerr << "caught exception: " << err.what() << std::endl;
		}
		catch(...)
		{
			BOOST_ASSERT(false);
		}
	}

	// throw completely unknown exception
	{
		poet::active_function<void ()> thrower(&throw_really_unknown);
		poet::future<void> result = thrower();
		try
		{
			static_cast<void>(result);
			BOOST_ASSERT(false);
		}
		catch(const poet::unknown_exception &err)
		{
			std::cerr << "caught exception: " << err.what() << std::endl;
		}
		catch(...)
		{
			BOOST_ASSERT(false);
		}
	}
	return 0;
}
