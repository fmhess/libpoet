/*
	A test program for futures and active object classes

	Author: Frank Hess <frank.hess@nist.gov>
	Begin: 2004-11
*/
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/thread.hpp>
#include <poet/active_object.hpp>
#include <poet/future.hpp>
#include <iostream>
#include <vector>

class ActiveIncrementer
{
public:
	poet::future<int> increment(poet::future<int> value)
	{
		poet::promise<int> returnValue;
		boost::shared_ptr<IncrementRequest> request(new IncrementRequest(&_servant,
			value, returnValue));
		_scheduler.post_method_request(request);
		return returnValue;
	}
private:
	class Servant
	{
	public:
		int increment(int value) {boost::this_thread::sleep(boost::posix_time::seconds(1)); return ++value;}
	};
	class IncrementRequest: public poet::method_request_base
	{
	public:
		IncrementRequest(Servant *servant, poet::future<int> inputValue, poet::promise<int> returnValue):
			_servant(servant), _inputValue(inputValue), _returnValue(returnValue)
		{
		}
		virtual void run() {_returnValue.fulfill(_servant->increment(_inputValue));}
		virtual poet::future<void> scheduling_guard() const
		{
			return _inputValue;
		}
	private:
		Servant *_servant;
		poet::future<int> _inputValue;
		poet::promise<int> _returnValue;
	};

	Servant _servant;
	poet::scheduler _scheduler;

};

int main()
{
	ActiveIncrementer inc1;
	ActiveIncrementer inc2;
	std::vector<poet::future<int> > results(10);
	unsigned i;
	std::cerr << "getting Futures..." << std::endl;
	for(i = 0; i < 2; ++i)
	{
		poet::future<int> temp = inc1.increment(i);
		results.at(i) = inc2.increment(temp);
	}
	std::cerr << "converting Futures to values..." << std::endl;
	for(i = 0; i < 2; ++i)
	{
		int value = results.at(i);
		std::cerr << "value from results[" << i << "] is " << value << std::endl;
	}
	return 0;
}
