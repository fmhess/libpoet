// Copyright (C) Frank Mori Hess 2007-2008
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <iostream>
#include <poet/future_barrier.hpp>
#include <poet/future_select.hpp>
#include <utility>
#include <vector>

void get_future(poet::future<void> f)
{
	f.get();
}

void myslot(bool *ran)
{
	*ran = true;
}


class my_iterating_combiner
{
public:
	my_iterating_combiner(unsigned *sum): _sum(sum)
	{}
	template<typename Iterator>
	unsigned operator()(Iterator begin, Iterator end)
	{
		Iterator it;
		for(it = begin; it != end; ++it)
		{
			*_sum += *it;
		}
		return *_sum;
	}
private:
	unsigned *_sum;
};

void combining_barrier_test()
{
	{
		std::vector<poet::promise<double> > promises;
		promises.push_back(poet::promise<double>());
		promises.push_back(poet::promise<double>());
		std::vector<poet::future<void> > futures;
		std::copy(promises.begin(), promises.end(), std::back_inserter(futures));
		bool combiner_run_flag = false;
		poet::future<void> all_ready = poet::future_combining_barrier_range<void>(
			boost::bind(&myslot, &combiner_run_flag), futures.begin(), futures.end());
		assert(all_ready.ready() == false);
		assert(combiner_run_flag == false);
		assert(all_ready.has_exception() == false);
		promises.at(0).fulfill(1.0);
		assert(all_ready.ready() == false);
		assert(combiner_run_flag == false);
		assert(all_ready.has_exception() == false);
		promises.at(1).fulfill(2);
		assert(all_ready.ready() == true);
		assert(combiner_run_flag == true);
		assert(all_ready.has_exception() == false);
	}
	// similar, but input futures not converted to void before passing to future_combining_barrier
	{
		std::vector<poet::promise<unsigned> > promises;
		promises.push_back(poet::promise<unsigned>());
		promises.push_back(poet::promise<unsigned>());
		std::vector<poet::future<unsigned> > futures;
		std::copy(promises.begin(), promises.end(), std::back_inserter(futures));
		unsigned combiner_sum = 0;
		poet::future<void> all_ready = poet::future_combining_barrier_range<void>(
			my_iterating_combiner(&combiner_sum), futures.begin(), futures.end());
		assert(all_ready.ready() == false);
		assert(combiner_sum == 0);
		assert(all_ready.has_exception() == false);
		promises.at(0).fulfill(1);
		assert(all_ready.ready() == false);
		assert(combiner_sum == 0);
		assert(all_ready.has_exception() == false);
		promises.at(1).fulfill(2);
		assert(all_ready.ready() == true);
		assert(all_ready.has_exception() == false);
		assert(combiner_sum == futures.at(0).get() + futures.at(1).get());
	}
	// similar, but now with non-void result type
	{
		std::vector<poet::promise<unsigned> > promises;
		promises.push_back(poet::promise<unsigned>());
		promises.push_back(poet::promise<unsigned>());
		std::vector<poet::future<unsigned> > futures;
		std::copy(promises.begin(), promises.end(), std::back_inserter(futures));
		unsigned combiner_sum = 0;
		poet::future<unsigned> all_ready = poet::future_combining_barrier_range<unsigned>(
			my_iterating_combiner(&combiner_sum), futures.begin(), futures.end());
		assert(all_ready.ready() == false);
		assert(combiner_sum == 0);
		assert(all_ready.has_exception() == false);
		promises.at(0).fulfill(1);
		assert(all_ready.ready() == false);
		assert(combiner_sum == 0);
		assert(all_ready.has_exception() == false);
		promises.at(1).fulfill(2);
		assert(all_ready.ready() == true);
		assert(all_ready.has_exception() == false);
		assert(combiner_sum == futures.at(0).get() + futures.at(1).get());
		assert(all_ready.get() == combiner_sum);
	}

}

int main()
{
	std::cerr << __FILE__ << "... ";
	{
		poet::promise<double> x_promise;
		poet::future<double> x(x_promise);
		poet::promise<short> y_promise;
		poet::future<short> y(y_promise);
		poet::future<void> all_ready = poet::future_barrier(x, y);
		bool slot_run_flag = false;
		all_ready.connect_update(boost::bind(&myslot, &slot_run_flag));
		assert(all_ready.ready() == false);
		assert(all_ready.has_exception() == false);
		boost::thread blocking_thread(boost::bind(&get_future, all_ready));
		boost::this_thread::sleep(boost::posix_time::millisec(200));
		x_promise.fulfill(1.0);
		assert(all_ready.ready() == false);
		assert(slot_run_flag == false);
		assert(all_ready.has_exception() == false);
		bool thread_done = blocking_thread.timed_join(boost::get_system_time());
		assert(thread_done == false);
		y_promise.fulfill(2);
		assert(all_ready.ready() == true);
		assert(slot_run_flag == true);
		assert(all_ready.has_exception() == false);
		blocking_thread.join();
	}

	{
		poet::promise<short> x_promise;
		poet::future<short> x(x_promise);
		poet::promise<short> y_promise;
		poet::future<short> y(y_promise);
		poet::future<short> any_ready = poet::future_select(x, y);
		assert(any_ready.ready() == false);
		assert(any_ready.has_exception() == false);
		bool slot_run_flag = false;
		any_ready.connect_update(boost::bind(&myslot, &slot_run_flag));
		boost::thread blocking_thread(boost::bind(&get_future, any_ready));
		boost::this_thread::sleep(boost::posix_time::millisec(200));
		bool thread_done = blocking_thread.timed_join(boost::get_system_time());
		assert(thread_done == false);
		assert(any_ready.ready() == false);
		assert(slot_run_flag == false);
		assert(any_ready.has_exception() == false);
		static const short x_value = 1;
		x_promise.fulfill(x_value);
		assert(any_ready.ready() == true);
		assert(slot_run_flag == true);
		assert(any_ready.has_exception() == false);
		blocking_thread.join();
		y_promise.fulfill(2);
		assert(any_ready.ready() == true);
		assert(any_ready.has_exception() == false);
		assert(any_ready.get() == x_value);
	}

	{
		poet::promise<short> x_promise;
		poet::future<short> x(x_promise);
		poet::promise<double> y_promise;
		poet::future<double> y(y_promise);
		poet::future<void> any_ready = poet::future_select<void>(x, y);
		assert(any_ready.ready() == false);
		assert(any_ready.has_exception() == false);
		boost::thread blocking_thread(boost::bind(&get_future, any_ready));
		boost::this_thread::sleep(boost::posix_time::millisec(200));
		bool thread_done = blocking_thread.timed_join(boost::get_system_time());
		assert(thread_done == false);
		static const short x_value = 1;
		x_promise.fulfill(x_value);
		assert(any_ready.ready() == true);
		assert(any_ready.has_exception() == false);
		blocking_thread.join();
		y_promise.fulfill(2.0);
		assert(any_ready.ready() == true);
		assert(any_ready.has_exception() == false);
	}

	combining_barrier_test();

	std::cerr << "OK\n";
	return 0;
}
