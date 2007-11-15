
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <iostream>
#include <poet/monitor_ptr.hpp>
#include <poet/monitor_base.hpp>
#include <unistd.h>

class Monitored: public poet::monitor_base
{
public:
	void waiting_function()
	{
		std::cerr << __FUNCTION__ << ": enter (mutex acquired)" << std::endl;
		usleep(1000000);
		std::cerr << __FUNCTION__ << ": waiting... (mutex released)" << std::endl;
		wait();
		std::cerr << __FUNCTION__ << ": wait complete (mutex acquired)" << std::endl;
		usleep(1000000);
		std::cerr << __FUNCTION__ << ": exit (mutex released)" << std::endl;
	}
	void notifying_function()
	{
		std::cerr << __FUNCTION__ << ": enter, notifying (mutex acquired)" << std::endl;
		notify_all();
		usleep(1000000);
		std::cerr << __FUNCTION__ << ": exit (mutex released)" << std::endl;
	}
	void another_function()
	{
		std::cerr << __FUNCTION__ << ": enter (mutex acquired)" << std::endl;
		usleep(1000000);
		std::cerr << __FUNCTION__ << ": exit (mutex released)" << std::endl;
	}
};

void thread0_function(poet::monitor_ptr<Monitored> mymon)
{
	mymon->waiting_function();
}

void thread1_function(poet::monitor_ptr<Monitored> mymon)
{
	usleep(500000);
	mymon->notifying_function();
	usleep(500000);
	mymon->another_function();
}

int main()
{
	poet::monitor_ptr<Monitored> mymon(new Monitored);
	boost::thread thread0(boost::bind(&thread0_function, mymon));
	boost::thread thread1(boost::bind(&thread1_function, mymon));
	thread0.join();
	thread1.join();
	return 0;
}
