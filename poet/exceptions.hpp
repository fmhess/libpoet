/*
	poet::future defines a templated future class which can be used,
	for example, to implement "active objects" and asynchronous function
	calls.  See the paper "Active Object, An Object Behavioral Pattern for
	Concurrent Programming." by R. Greg Lavender and Douglas C. Schmidt
	for more information about active objects and futures.

	Active objects that use futures for both input parameters and
	return values can be chained together in pipelines or do
	dataflow-like processing, thereby achieving good concurrency.

	begin: Frank Hess <frank.hess@nist.gov>  2007-01-22
*/
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_EXCEPTIONS_H
#define _POET_EXCEPTIONS_H

namespace poet
{
	class cancelled_future: public std::runtime_error
	{
	public:
		cancelled_future(): std::runtime_error("poet::cancelled_future")
		{}
		virtual ~cancelled_future() throw() {}
	};
	class uncertain_future: public std::runtime_error
	{
	public:
		uncertain_future(): std::runtime_error("poet::uncertain_future")
		{}
		virtual ~uncertain_future() throw() {}
	};
	class unknown_exception: public std::runtime_error
	{
	public:
		unknown_exception(const std::string &description = "poet::unknown_exception"):
			runtime_error(description)
		{}
		virtual ~unknown_exception() throw() {}
	};
}

#endif // _POET_EXCEPTIONS_H
