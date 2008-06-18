// Copyright (C) Frank Mori Hess 2008
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <boost/tuple/tuple.hpp>
#include <boost/variant/variant.hpp>
#include <iterator>
#include <poet/future_barrier.hpp>
#include <poet/future_select.hpp>
#include <vector>

struct identity
{
	template<typename T>
	const T& operator()(const T &t) const
	{
		return t;
	}
};

struct tuple_creator
{
	template<typename T1, typename T2>
	boost::tuple<T1, T2> operator()(const T1 &a1, const T2 &a2)
	{
		return boost::tuple<T1, T2>(a1, a2);
	}
};

template<typename T1, typename T2>
poet::future<boost::tuple<T1, T2> > operator&&(const poet::future<T1> &f1, const poet::future<T2> &f2)
{
	return poet::future_combining_barrier<boost::tuple<T1, T2> >(tuple_creator(), identity(), f1, f2);
}

template<typename T1, typename T2>
poet::future<boost::variant<T1, T2> > operator||(const poet::future<T1> &f1, const poet::future<T2> &f2)
{
	typedef boost::variant<T1, T2> result_type;
	poet::future<result_type> variant_f1 = poet::future_combining_barrier<result_type>(identity(), identity(), f1);
	poet::future<result_type> variant_f2 = poet::future_combining_barrier<result_type>(identity(), identity(), f2);
	return poet::future_select(variant_f1, variant_f2);
}

#include <cassert>
#include <iostream>
#include <string>
#include <boost/variant.hpp>

struct mycombiner
{
	template<typename Iterator>
	int operator()(Iterator first, Iterator last)
	{
		return 1;
	}
};

void barrier_exception_test()
{
	{
		poet::promise<int> p1;
		poet::future<int> f1 = p1;
		poet::promise<int> p2;
		poet::future<int> f2 = p2;
		poet::future<void> result = poet::future_barrier(f1, f2);
		p1.renege(std::runtime_error("test error"));
		assert(result.has_exception());
		assert(result.ready() == false);
		try
		{
			result.get();
			assert(false);
		}
		catch(const std::runtime_error &)
		{}
	}
	// again, this time with future_combining_barrier_range
	{
		std::vector<poet::promise<int> > promises;
		promises.push_back(poet::promise<int>());
		promises.push_back(poet::promise<int>());
		std::vector<poet::future<int> > futures;
		std::copy(promises.begin(), promises.end(), std::back_inserter(futures));
		poet::future<int> result = poet::future_combining_barrier_range<int>(mycombiner(), poet::detail::identity(), futures.begin(), futures.end());
		promises.front().renege(std::runtime_error("test error"));
		assert(result.has_exception());
		assert(result.ready() == false);
		try
		{
			result.get();
			assert(false);
		}
		catch(const std::runtime_error &)
		{}
	}
	// again, this time with future_combining_barrier
	{
		poet::promise<int> p1;
		poet::promise<int> p2;
		poet::future<int> f1 = p1;
		poet::future<int> f2 = p2;
		poet::future<boost::tuple<int, int> > result = f1 && f2;
		p1.renege(std::runtime_error("test error"));
		assert(result.has_exception());
		assert(result.ready() == false);
		try
		{
			result.get();
			assert(false);
		}
		catch(const std::runtime_error &)
		{}
	}
}

int main()
{
	std::cerr << __FILE__ << "... ";

	poet::promise<int> pi;
	poet::promise<double> pd;
	poet::promise<std::string> pstr;
	poet::promise<std::string> pstr2;
	poet::future<int> fi = pi;
	poet::future<double> fd = pd;
	poet::future<std::string> fstr = pstr;
	poet::future<std::string> fstr2 = pstr2;

	poet::future<boost::tuple<int, double> > fall = fi && fd;
	poet::future<boost::variant<boost::tuple<int, double>, std::string > > fany = fall || fstr;
	poet::future<boost::variant<boost::tuple<int, double>, std::string > > fany2 = fall || fstr2;

	assert(fall.ready() == false);
	assert(fany.ready() == false);
	assert(fany2.ready() == false);

	pi.fulfill(1);
	assert(fall.ready() == false);

	assert(fany.ready() == false);
	assert(fstr.ready() == false);
	pstr.fulfill("hello");
	assert(fany.ready() == true);
	assert(fstr.ready() == true);

	assert(fall.ready() == false);
	assert(fany2.ready() == false);
	pd.fulfill(1.5);
	assert(fall.ready() == true);
	assert(fany2.ready() == true);

	assert(fall.get().get<0>() == 1);
	assert(std::abs(fall.get().get<1>() / 1.5 - 1.0) < 1e-6);
	assert(boost::get<std::string>(fany.get()) == "hello");
	boost::tuple<int, double> fany2_value = boost::get<boost::tuple<int, double> >(fany2.get());
	assert(fany2_value.get<0>() == 1);

	barrier_exception_test();

	std::cerr << "OK" << std::endl;
	return 0;
}
