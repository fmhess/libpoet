/*
	begin: Frank Hess <frank.hess@nist.gov>  2007-01-22
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <poet/active_object.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread_time.hpp>
#include <cassert>
#include <poet/detail/nonvoid.hpp>
#include <poet/future_barrier.hpp>
#include <poet/future_select.hpp>

namespace poet
{
	namespace detail
	{
		inline boost::shared_ptr<method_request_base> method_request_future_combiner(const boost::shared_ptr<method_request_base> &request, null_type)
		{
			return request;
		};
		inline exception_ptr method_request_future_exception_handler(const boost::shared_ptr<method_request_base> &request,
			const exception_ptr &)
		{
			exception_ptr ep = poet::copy_exception(request);
			return ep;
		};

		inline future<boost::shared_ptr<method_request_base> > make_method_request_future(
			const boost::shared_ptr<method_request_base> &request)
		{
			BOOST_ASSERT(request);
			return future_combining_barrier<boost::shared_ptr<method_request_base> >(
				boost::bind(&method_request_future_combiner, request, _1),
				boost::bind(&method_request_future_exception_handler, request, _1),
				request->scheduling_guard());
		}
	}

	void in_order_activation_queue::push_back(const boost::shared_ptr<method_request_base> &request)
	{
		boost::unique_lock<boost::mutex> lock(_mutex);
		if(_pendingRequests.empty())
			_next_method_request.fulfill(detail::make_method_request_future(request));
		_pendingRequests.push_back(request);
	}

	boost::shared_ptr<method_request_base> in_order_activation_queue::get_request()
	{
		future<boost::shared_ptr<method_request_base> > next;
		{
			boost::unique_lock<boost::mutex> lock(_mutex);
			next = future_select<boost::shared_ptr<method_request_base> >(_next_method_request, _wake_promise);
		}
		boost::shared_ptr<method_request_base> result;
		try
		{
			result = next.get();
		}
		catch(const boost::shared_ptr<method_request_base> &method_request)
		{
			result = method_request;
		}
		catch(const std::exception &err)
		{
			std::cerr << "caught unexpected exception: " << err.what() << std::endl;
			BOOST_ASSERT(false);
		}
		catch(...)
		{
			BOOST_ASSERT(false);
		}
		if(result)
		{
			boost::unique_lock<boost::mutex> lock(_mutex);
			_pendingRequests.pop_front();
			_next_method_request.reset();
			if(_pendingRequests.empty() == false)
				_next_method_request.fulfill(detail::make_method_request_future(_pendingRequests.front()));
		}
		return result;
	}

	void in_order_activation_queue::wake()
	{
		boost::unique_lock<boost::mutex> lock(_mutex);
		_wake_promise.fulfill(boost::shared_ptr<method_request_base>());
		_wake_promise.reset();
	}

	void out_of_order_activation_queue::push_back(const boost::shared_ptr<method_request_base> &request)
	{
		_selector.push(detail::make_method_request_future(request));
	}

	boost::shared_ptr<method_request_base> out_of_order_activation_queue::get_request()
	{
		future<boost::shared_ptr<method_request_base> > next;
		{
			boost::unique_lock<boost::mutex> lock(_mutex);
			next = future_select<boost::shared_ptr<method_request_base> >(_selector.selected(), _wake_promise);
		}
		boost::shared_ptr<method_request_base> result;
		try
		{
			result = next.get();
		}
		catch(const boost::shared_ptr<method_request_base> &method_request)
		{
			result = method_request;
		}
		catch(...)
		{
			BOOST_ASSERT(false);
		}
		if(result)
		{
			_selector.pop_selected();
		}
		return result;
	}

	void out_of_order_activation_queue::wake()
	{
		boost::unique_lock<boost::mutex> lock(_mutex);
		_wake_promise.fulfill(boost::shared_ptr<method_request_base>());
		_wake_promise.reset();
	}

	namespace detail
	{
		// scheduler_impl

		scheduler_impl::scheduler_impl(const boost::shared_ptr<activation_queue_base> &activationQueue):
			_activationQueue(activationQueue), _mortallyWounded(false),
			_detached(false)
		{
		}

		void scheduler_impl::post_method_request(const boost::shared_ptr<method_request_base> &methodRequest)
		{
			_activationQueue->push_back(methodRequest);
		}

		bool scheduler_impl::dispatch()
		{
			boost::shared_ptr<method_request_base> methodRequest = _activationQueue->get_request();
			if(methodRequest) methodRequest->run();
			return methodRequest;
		}

		void scheduler_impl::dispatcherThreadFunction(const boost::shared_ptr<scheduler_impl> &shared_this_in)
		{
			/* shared_this insures scheduler_impl object is not destroyed while its scheduler thread is still
			running. */
			boost::shared_ptr<scheduler_impl> shared_this = shared_this_in;
			while(shared_this->mortallyWounded() == false)
			{
				boost::shared_ptr<method_request_base> next_request = shared_this->_activationQueue->get_request();
				if(shared_this->mortallyWounded()) break;
				try
				{
					if(next_request) next_request->run();
				}
				catch(...)
				{
					BOOST_ASSERT(false);
				}
				if(shared_this->detached() && shared_this->_activationQueue->empty())
				{
					break;
				}
			}
		}

		void scheduler_impl::kill()
		{
			{
				boost::mutex::scoped_lock lock(_mutex);
				_mortallyWounded = true;
			}
			_activationQueue->wake();
		}

		bool scheduler_impl::mortallyWounded() const
		{
			boost::mutex::scoped_lock lock(_mutex);
			return _mortallyWounded;
		}

		void scheduler_impl::detach()
		{
			{
				boost::mutex::scoped_lock lock(_mutex);
				_detached = true;
			}
			_activationQueue->wake();
		}
	} // namespace detail

	// scheduler

	scheduler::scheduler(const boost::shared_ptr<activation_queue_base> &activationQueue):
		_pimpl(new detail::scheduler_impl(activationQueue))
	{
		_dispatcherThread.reset(new boost::thread(boost::bind(&detail::scheduler_impl::dispatcherThreadFunction, _pimpl)));
	}

	void scheduler::join()
	{
		BOOST_ASSERT(_pimpl->mortallyWounded());
		boost::thread thisThread;
		if(thisThread == *_dispatcherThread)
		{
			throw std::invalid_argument("Cannot join scheduler thread from scheduler thread.");
		}
		_dispatcherThread->join();
	}
}	// namespace poet
