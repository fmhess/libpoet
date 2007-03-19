/*
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

#include <poet/active_object.hpp>

#include <boost/bind.hpp>
#include <cassert>

void poet::in_order_activation_queue::push_back(const boost::shared_ptr<method_request_base> &request)
{
	boost::mutex::scoped_lock lock(_mutex);
	_pendingRequests.push_back(request);
}

boost::shared_ptr<poet::method_request_base> poet::in_order_activation_queue::getRequest()
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

boost::shared_ptr<poet::method_request_base> poet::out_of_order_activation_queue::getRequest()
{
	boost::mutex::scoped_lock lock(_mutex);
	/* Optimization: before going through the entire list in reverse, do a quick check
	of the common case where the request at the front of the queue is ready */
	boost::shared_ptr<method_request_base> methodRequest = in_order_activation_queue::unlockedGetRequest();
	if(methodRequest)
	{
		return methodRequest;
	}
	/* Iterating in reverse through the queue ensures that if we
	pick a methodRequest to execute, it was the oldest ready
	request at some point in time.  If you iterate forwards,
	then there is a race between going through the queue and
	requests asyncronously becoming ready.  The race makes it
	possible for a request to be chosen to run when there is another
	request which both older and has been ready longer. */
	list_type::reverse_iterator rit;
	list_type::iterator readyIt = _pendingRequests.end();
	for(rit = _pendingRequests.rbegin(); rit != _pendingRequests.rend(); ++rit)
	{
		while(rit != _pendingRequests.rend() && (*rit)->cancelled())
		{
			// need the predecrement to get an iterator that dereferences to the same object
			list_type::iterator eraseIt = --rit.base();
			/* rit is still a valid reverse iterator after erase, since
			it is based on an iterator one past the erased point.  The erase
			does have the side-effect of incrementing rit */
			_pendingRequests.erase(eraseIt);
		}
		if(rit != _pendingRequests.rend() && (*rit)->ready())
		{
			// need the predecrement to get an iterator that dereferences to the same object
			readyIt = --rit.base();
		}
	}
	if(readyIt != _pendingRequests.end())
	{
		methodRequest = *readyIt;
		_pendingRequests.erase(readyIt);
	}
	return methodRequest;
}

// scheduler_impl

poet::detail::scheduler_impl::scheduler_impl(int millisecTimeout,
	const boost::shared_ptr<activation_queue_base> &activationQueue):
	_activationQueue(activationQueue), _mortallyWounded(false), _millisecTimeout(millisecTimeout),
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
	boost::shared_ptr<method_request_base> methodRequest = _activationQueue->getRequest();
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
			static const unsigned milliPerUnit = 1000;
			static const unsigned nanoPerMilli = 1000000;
			static const unsigned nanoPerUnit = 1000000000;
			unsigned seconds = shared_this->_millisecTimeout / milliPerUnit;
			unsigned nanoseconds = (shared_this->_millisecTimeout - seconds * milliPerUnit) * nanoPerMilli;
			seconds += nanoseconds / nanoPerUnit;
			nanoseconds %= nanoPerUnit;
			boost::xtime wakeTime;
			boost::xtime_get(&wakeTime, boost::TIME_UTC);
			wakeTime.sec += seconds;
			wakeTime.nsec += nanoseconds;
			wakeTime.sec += wakeTime.nsec / nanoPerUnit;
			wakeTime.nsec %= nanoPerUnit;
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
