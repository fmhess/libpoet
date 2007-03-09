/*
	A test program for futures and active object classes

	Author: Frank Hess <frank.hess@nist.gov>
	Begin: 2004-11
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
#include <poet/future.hpp>
#include <iostream>
#include <vector>
#include <unistd.h>

class ActiveIncrementer
{
public:
	poet::future<int> increment(poet::future<int> value)
	{
		poet::future<int> returnValue;
		boost::shared_ptr<IncrementRequest> request(new IncrementRequest(&_servant,
			value, returnValue));
		_scheduler.post_method_request(request);
		return returnValue;
	}
private:
	class Servant
	{
	public:
		int increment(int value) {sleep(1); return ++value;}
	};
	class IncrementRequest: public poet::method_request_base
	{
	public:
		IncrementRequest(Servant *servant, poet::future<int> inputValue, poet::future<int> returnValue):
			_servant(servant), _inputValue(inputValue), _returnValue(returnValue)
		{
			_readyConnection = inputValue.connectUpdate(boost::bind(boost::ref(_updateSignal)));
		}
		virtual void run() {_returnValue = _servant->increment(_inputValue);}
		virtual bool ready() const {return _inputValue.ready();}
	private:
		Servant *_servant;
		poet::future<int> _inputValue;
		poet::future<int> _returnValue;
		boost::signalslib::scoped_connection _readyConnection;
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
	for(i = 0; i < 10; ++i)
	{
		poet::future<int> temp = inc1.increment(i);
		results.at(i) = inc2.increment(temp);
	}
	std::cerr << "converting Futures to values..." << std::endl;
	for(i = 0; i < 10; ++i)
	{
		int value = results.at(i);
		std::cerr << "value from results[" << i << "] is " << value << std::endl;
	}
	return 0;
}
