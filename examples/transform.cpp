// An example which passes an active_function as the functor
// used by std::transform, in order to perform a transformation
// on containers of futures.

// Copyright (C) Frank Mori Hess 2008
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <cmath>
#include <functional>
#include <iostream>
#include <poet/active_function.hpp>
#include <vector>

int main()
{
	unsigned i;
	const unsigned vector_length = 10;
	// input data, assigned actual values later
	std::vector<poet::promise<double> > x_promises;
	std::vector<poet::promise<double> > y_promises;
	// we push the promises on onto the std::vector one at a time to make
	// sure the std::vector doesn't copy-construct them from each other.
	for(i = 0; i < vector_length; ++i)
	{
		x_promises.push_back(poet::promise<double>());
		y_promises.push_back(poet::promise<double>());
	}

	// active_functions
	std::plus<double> passive_adder;
	poet::active_function<double (double, double)> active_adder(passive_adder);

	std::vector<poet::future<double> > x;
	std::copy(x_promises.begin(), x_promises.end(), std::back_inserter(x));
	std::vector<poet::future<double> > y;
	std::copy(y_promises.begin(), y_promises.end(), std::back_inserter(y));
	std::vector<poet::future<double> > sum;

	std::transform(x.begin(), x.end(), y.begin(), std::back_inserter(sum), active_adder);

	// fulfill input promises
	for(i = 0; i < x_promises.size(); ++i)
	{
		x_promises.at(i).fulfill(i % 4);
		y_promises.at(i).fulfill(i % 3);
	}

	// wait for futures to become ready and convert them to values
	for(i = 0; i < sum.size(); ++i)
	{
		std::cout << "sum " << i << " = " << sum.at(i).get() << "\n";
	}

	return 0;
}
