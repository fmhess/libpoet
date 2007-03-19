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

#ifndef _POET_EXCEPTIONS_H
#define _POET_EXCEPTIONS_H

namespace poet
{
	/*!  \brief Exception thrown by a cancelled future.

	This exception is thrown when an attempt to convert a future to
	its associated value fails due to future::cancel() being called
	on a future that references the same promise.
	*/
	class cancelled_future: public std::runtime_error
	{
	public:
		/*! Constructor. */
		cancelled_future(): std::runtime_error("poet::cancelled_future")
		{}
		/*! Virtual destructor. */
		virtual ~cancelled_future() throw() {}
	};
	/*!  \brief Exception thrown by an uncertain future.

	This exception is thrown when an attempt is made to convert a
	future with no promise into its associated value.  This
	can happen if the future was default-constructed, or
	its associated promise object has been destroyed without
	being fulfilled.
	*/
	class uncertain_future: public std::runtime_error
	{
	public:
		/*! Constructor. */
		uncertain_future(): std::runtime_error("poet::uncertain_future")
		{}
		/*! Virtual destructor. */
		virtual ~uncertain_future() throw() {}
	};
	/*!  \brief Exception used as a placeholder for unknown exceptions.

	Exceptions unknown by the current_exception() code are replaced
	with this class.  It is also used to replace exceptions whose
	exact type is unknown but which are derived from std::exception, in which
	case the what() string will be made to match the what() string of the
	original unknown exception.
	*/
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
