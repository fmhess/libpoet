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

void poet::in_order_activation_queue::push_back(const boost::shared_ptr<method_request_base> &request)
{
	boost::mutex::scoped_lock lock(_mutex);
	_pendingRequests.push_back(request);
}

boost::shared_ptr<poet::method_request_base> poet::in_order_activation_queue::get_request()
{
	boost::mutex::scoped_lock lock(_mutex);
	return unlockedGetRequest();
}

boost::shared_ptr<poet::method_request_base> poet::in_order_activation_queue::unlockedGetRequest()
{
	boost::shared_ptr<method_request_base> methodRequest;
	list_type::iterator it = _pendingRequests.begin();
	while(it != _pendingRequests.end() && (*it)->cancelled())
	{
		it = _pendingRequests.erase(it);
	}
	if(it != _pendingRequests.end())
	{
		if((*it)->ready())
		{
			methodRequest = *it;
			_pendingRequests.erase(it);
		}
	}
	return methodRequest;
}

void poet::out_of_order_activation_queue::push_back(const boost::shared_ptr<method_request_base> &request)
{
	boost::mutex::scoped_lock lock(_mutex);
	_pendingRequests.insert(_next, request);
}

boost::shared_ptr<poet::method_request_base> poet::out_of_order_activation_queue::get_request()
{
	boost::mutex::scoped_lock lock(_mutex);
	boost::shared_ptr<method_request_base> methodRequest;
	list_type::iterator old_next = _next;
	while(_next != _pendingRequests.end())
	{
		if((*_next)->cancelled())
		{
			_next = _pendingRequests.erase(_next);
		}else if((*_next)->ready())
		{
			methodRequest = *_next;
			_next = _pendingRequests.erase(_next);
			return methodRequest;
		}else
		{
			++_next;
		}
	}
	_next = _pendingRequests.begin();
	while(_next != old_next)
	{
		if((*_next)->cancelled())
		{
			_next = _pendingRequests.erase(_next);
		}else if((*_next)->ready())
		{
			methodRequest = *_next;
			_next = _pendingRequests.erase(_next);
			return methodRequest;
		}else
		{
			++_next;
		}
	}
	return methodRequest;
}

// scheduler_impl

poet::detail::scheduler_impl::scheduler_impl(int millisecTimeout,
	const boost::shared_ptr<activation_queue_base> &activationQueue):
	_activationQueue(activationQueue), _wakePending(false), _mortallyWounded(false), _millisecTimeout(millisecTimeout),
	_detached(false)
{
}

void poet::detail::scheduler_impl::post_method_request(const boost::shared_ptr<method_request_base> &methodRequest)
{
	_activationQueue->push_back(methodRequest);
	methodRequest->connect_update(boost::bind(&poet::detail::scheduler_impl::wake, this));
	wake();
}

void poet::detail::scheduler_impl::wake()
{
	setWakePending(true);
	_wakeCondition.locking_notify_all();
}

bool poet::detail::scheduler_impl::dispatch()
{
	boost::shared_ptr<method_request_base> methodRequest = _activationQueue->get_request();
	if(methodRequest) methodRequest->run();
	return methodRequest;
}

void poet::detail::scheduler_impl::dispatcherThreadFunction(const boost::shared_ptr<scheduler_impl> &shared_this_in)
{
	/* shared_this insures scheduler_impl object is not destroyed while its scheduler thread is still
	running. */
	boost::shared_ptr<scheduler_impl> shared_this = shared_this_in;
	while(shared_this->mortallyWounded() == false)
	{
		if(shared_this->_millisecTimeout > 0)
		{
			boost::posix_time::time_duration timeout = boost::posix_time::millisec(shared_this->_millisecTimeout);
			boost::system_time wakeTime = boost::get_system_time() +
				boost::posix_time::millisec(shared_this->_millisecTimeout);
			shared_this->_wakeCondition.locking_timed_wait(wakeTime, boost::bind(&poet::detail::scheduler_impl::wakePending, shared_this.get()));
		}else if(shared_this->_millisecTimeout < 0)
		{
			shared_this->_wakeCondition.locking_wait(boost::bind(&poet::detail::scheduler_impl::wakePending, shared_this.get()));
		}
		shared_this->setWakePending(false);
		if(shared_this->mortallyWounded()) break;
		while(shared_this->dispatch());
		if(shared_this->detached() && shared_this->_activationQueue->empty()) break;
	}
}

void poet::detail::scheduler_impl::kill()
{
	{
		boost::mutex::scoped_lock lock(_mutex);
		_mortallyWounded = true;
	}
	wake();
}

bool poet::detail::scheduler_impl::mortallyWounded() const
{
	boost::mutex::scoped_lock lock(_mutex);
	return _mortallyWounded;
}

bool poet::detail::scheduler_impl::wakePending() const
{
	boost::mutex::scoped_lock lock(_mutex);
	return _wakePending ;
}

void poet::detail::scheduler_impl::setWakePending(bool value)
{
	boost::mutex::scoped_lock lock(_mutex);
	_wakePending = value;
}

void poet::detail::scheduler_impl::detach()
{
	{
		boost::mutex::scoped_lock lock(_mutex);
		_detached = true;
	}
	wake();
}

// scheduler

poet::scheduler::scheduler(int millisecTimeout,
	const boost::shared_ptr<activation_queue_base> &activationQueue):
	_pimpl(new detail::scheduler_impl(millisecTimeout, activationQueue))
{
	_dispatcherThread.reset(new boost::thread(boost::bind(&poet::detail::scheduler_impl::dispatcherThreadFunction, _pimpl)));
}

void poet::scheduler::join()
{
	BOOST_ASSERT(_pimpl->mortallyWounded());
	boost::thread thisThread;
	if(thisThread == *_dispatcherThread)
	{
		throw std::invalid_argument("Cannot join scheduler thread from scheduler thread.");
	}
	_dispatcherThread->join();
}
