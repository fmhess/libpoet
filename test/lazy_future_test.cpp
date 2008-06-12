// Copyright (C) Frank Mori Hess 2008
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <cassert>
#include <poet/future_barrier.hpp>
#include <poet/future_select.hpp>
#include <poet/detail/identity.hpp>
#include <iostream>


int should_not_run(int )
{
	assert(false);
	return 0;
}

int negate(int x)
{
	return -x;
}

void lazy_future_select_test()
{
	static const int test_value = 3;

	poet::promise<int> p0;
	poet::future<int> f0 = p0;
	poet::future<int> f0_combined = poet::future_combining_barrier<int>(&negate, poet::detail::identity(), f0);

	poet::promise<int> p1;
	poet::future<int> f1 = p1;
	poet::future<int> f1_combined = poet::future_combining_barrier<int>(&should_not_run, poet::detail::identity(), f1);

	poet::future<int> fany = poet::future_select(f0_combined, f1_combined);
	p0.fulfill(test_value);
	assert(fany.ready());
	assert(fany.get() == -test_value);

	p1.fulfill(1);
	// further usage of fany should not result in f1_combined being waited on or its combiner running
	fany.ready();
	fany.get();
}

// same as lazy_future_select_test except using future_selector object instead of future_select function
void lazy_future_selector_test()
{
	static const int test_value = 3;

	poet::promise<int> p0;
	poet::future<int> f0 = p0;
	poet::future<int> f0_combined = poet::future_combining_barrier<int>(&negate, poet::detail::identity(), f0);

	poet::promise<int> p1;
	poet::future<int> f1 = p1;
	poet::future<int> f1_combined = poet::future_combining_barrier<int>(&should_not_run, poet::detail::identity(), f1);

	poet::future_selector<int> fany;
	fany.push(f1_combined);
	fany.push(f0_combined);

	p0.fulfill(test_value);
	assert(fany.selected().ready());
	assert(fany.selected().get() == -test_value);

	p1.fulfill(1);
	/* further usage of completed future from fany.selected() should not result in
	f1_combined being waited on or its combiner running */
	fany.selected().ready();
	fany.selected().get();
}

int main()
{
	std::cerr << __FILE__ << "... ";

	lazy_future_select_test();
	lazy_future_selector_test();

	std::cerr << "OK\n";
	return 0;
}
