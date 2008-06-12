// Copyright (C) Frank Mori Hess 2007-2008
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <iostream>
#include <poet/detail/identity.hpp>
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

boost::tuple<unsigned, double> my_hetero_combiner(unsigned arg1, poet::null_type, double arg3)
{
	boost::tuple<unsigned, double> result;
	result.get<0>() = arg1;
	result.get<1>() = arg3;
	return result;
}

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
			boost::bind(&myslot, &combiner_run_flag), poet::detail::identity(), futures.begin(), futures.end());
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
			my_iterating_combiner(&combiner_sum), poet::detail::identity(), futures.begin(), futures.end());
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
			my_iterating_combiner(&combiner_sum), poet::detail::identity(), futures.begin(), futures.end());
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
	// now with heterogeneous inputs
	{
		static const unsigned unsigned_test_value = 1;
		static const double double_test_value = 1.5;
		static const double epsilon = 1e-6;

		poet::promise<unsigned> promise0;
		poet::promise<void> promise1;
		poet::promise<double> promise2;
		poet::future<unsigned> future0 = promise0;
		poet::future<void> future1 = promise1;
		poet::future<double> future2 = promise2;
		poet::future<boost::tuple<unsigned, double> > all_ready =
			poet::future_combining_barrier<boost::tuple<unsigned, double> >(
				&my_hetero_combiner, poet::detail::identity(), future0, future1, future2);
		assert(all_ready.ready() == false);
		assert(all_ready.has_exception() == false);
		promise0.fulfill(unsigned_test_value);
		assert(all_ready.ready() == false);
		assert(all_ready.has_exception() == false);
		promise2.fulfill(double_test_value);
		assert(all_ready.ready() == false);
		assert(all_ready.has_exception() == false);
		promise1.fulfill();
		assert(all_ready.ready() == true);
		assert(all_ready.has_exception() == false);
		assert(all_ready.get().get<0>() == unsigned_test_value);
		assert(std::abs(all_ready.get().get<1>() / double_test_value - 1.0) < epsilon);
	}
}

void select_none_test()
{
	std::vector<poet::future<int> > futures;
	poet::future<int> selected_future = poet::future_select_range(futures.begin(), futures.end());
	assert(selected_future.ready() == false);
	assert(selected_future.has_exception());
	try
	{
		selected_future.get();
		assert(false);
	}
	catch(const poet::uncertain_future&)
	{}
	catch(...)
	{
		assert(false);
	}
}

void barrier_none_test()
{
	{
		std::vector<poet::future<int> > futures;
		poet::future<void> result = poet::future_barrier_range(futures.begin(), futures.end());
		assert(result.ready());
		assert(result.has_exception() == false);
		try
		{
			result.get();
		}
		catch(...)
		{
			assert(false);
		}
	}
	// again, but with future_combining_barrier_range
	{
		std::vector<poet::future<unsigned> > futures;
		unsigned sum = 0;
		poet::future<unsigned> result = poet::future_combining_barrier_range<unsigned>(my_iterating_combiner(&sum),
			poet::detail::identity(),
			futures.begin(), futures.end());
		assert(result.ready());
		assert(result.has_exception() == false);
		try
		{
			assert(result.get() == sum);
		}
		catch(...)
		{
			assert(false);
		}
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
		assert(all_ready.ready() == false);
		assert(all_ready.has_exception() == false);
		boost::thread blocking_thread(boost::bind(&get_future, all_ready));
		boost::this_thread::sleep(boost::posix_time::millisec(200));
		x_promise.fulfill(1.0);
		assert(all_ready.ready() == false);
		assert(all_ready.has_exception() == false);
		bool thread_done = blocking_thread.timed_join(boost::get_system_time());
		assert(thread_done == false);
		y_promise.fulfill(2);
		assert(all_ready.ready() == true);
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
		boost::thread blocking_thread(boost::bind(&get_future, any_ready));
		boost::this_thread::sleep(boost::posix_time::millisec(200));
		bool thread_done = blocking_thread.timed_join(boost::get_system_time());
		assert(thread_done == false);
		assert(any_ready.ready() == false);
		assert(any_ready.has_exception() == false);
		static const short x_value = 1;
		x_promise.fulfill(x_value);
		assert(any_ready.ready() == true);
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
	select_none_test();
	barrier_none_test();

	std::cerr << "OK\n";
	return 0;
}
