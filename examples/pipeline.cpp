// A toy example that calculates the length of a series of
// 2 dimensional vectors concurrently using 4 threads.
// A real program more concerned with performance
// would perform less trivial operations in the active_functions,
// to reduce the active_function calling overhead.

// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/array.hpp>
#include <cmath>
#include <functional>
#include <iostream>
#include <poet/active_function.hpp>
#include <vector>

double square_root(double a)
{
	return std::sqrt(a);
}

int main()
{
	static const unsigned num_vectors = 10;
	unsigned i;

	// input data, assigned actual values later
	std::vector<boost::array<poet::promise<double>, 2> > promises;
	// we push the promises on onto the std::vector one at a time to make
	// sure the std::vector doesn't copy-construct them from each other.
	for(i = 0; i < num_vectors; ++i)
	{
		promises.push_back(boost::array<poet::promise<double>, 2>());
	}

	// active_functions
	poet::active_function<double (double, double)> active_adder((std::plus<double>()));
	poet::active_function<double (double, double)> active_multiplier0((std::multiplies<double>()));
	poet::active_function<double (double, double)> active_multiplier1((std::multiplies<double>()));
	poet::active_function<double (double)> active_sqrt(&square_root);

	std::vector<poet::future<double> > lengths;
	// pass promises through our active_function pipeline
	for(i = 0; i < promises.size(); ++i)
	{
		poet::future<double> product0 = active_multiplier0(promises.at(i).at(0), promises.at(i).at(0));
		poet::future<double> product1 = active_multiplier1(promises.at(i).at(1), promises.at(i).at(1));

		poet::future<double> sum = active_adder(product0, product1);

		poet::future<double> root = active_sqrt(sum);

		lengths.push_back(root);
	}

	// fulfill input promises
	for(i = 0; i < promises.size(); ++i)
	{
		promises.at(i).at(0).fulfill(i % 4);
		promises.at(i).at(1).fulfill(i % 3);
	}

	// wait for futures to become ready and convert them to values
	for(i = 0; i < lengths.size(); ++i)
	{
		boost::array<poet::future<double>, 2> vec;
		vec.at(0) = promises.at(i).at(0);
		vec.at(1) = promises.at(i).at(1);
		double value = lengths.at(i);
		std::cout << "vector " << i << " = {" << vec.at(0) << ", " <<
			vec.at(1) << "}, length = " << value << "\n";
	}

	return 0;
}
