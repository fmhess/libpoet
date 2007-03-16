
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <poet/future.hpp>
#include <iostream>
#include <vector>
#include <unistd.h>

static void delayed_increment(poet::promise<int> mypromise, poet::future<int> value)
{
	sleep(1);
	int intValue = value;
	++intValue;
	std::cerr << "Setting future value to " << intValue << std::endl;
	mypromise.fulfill(intValue);
};

static poet::future<int> async_delayed_increment(poet::future<int> value)
{
	poet::promise<int> mypromise;
	boost::thread mythread(boost::bind(&delayed_increment, mypromise, value));
	return mypromise;
}

int main()
{
	std::vector<poet::future<int> > futures;
	unsigned i;
	std::cerr << "getting Futures..." << std::endl;
	for(i = 0; i < 10; ++i)
	{
		if(i == 0)
		{
			futures.push_back(async_delayed_increment(0));
		}else
		{
			futures.push_back(async_delayed_increment(futures.at(i - 1)));
		}
	}
	std::cerr << "converting Futures to values..." << std::endl;
	for(i = 0; i < 10; ++i)
	{
		int value = futures.at(i);
		std::cerr << "value from futures[" << i << "] is " << value << std::endl;
	}

	std::cerr << "trying to extract future element from future<vector>..." << std::endl;
	std::vector<double> myVec;
	myVec.push_back(0.5);
	myVec.push_back(1.1);
	std::cerr << "original vector element 1 is " << myVec.at(1) << std::endl;
	poet::promise<std::vector<double> > myVecPromise;
	poet::future<std::vector<double> > myVecFuture(myVecPromise);
	poet::future<double> myVecElementFuture(myVecFuture, boost::bind<const double&>(&std::vector<double>::at, _1, 1));
	myVecPromise.fulfill(myVec);
	double myVecElement = myVecElementFuture;
	std::cerr << "future vector element 1 is " << myVecElement << std::endl;

	return 0;
}
