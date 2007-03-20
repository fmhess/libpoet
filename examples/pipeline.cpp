// A toy example that calculates the length of a series of
// 2 dimensional vectors concurrently using 4 threads.
// A real program more concerned with performance
// would perform less trivial operations in the active_functions,
// to reduce the overhead of passing values between active_functions
// with futures.

// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/array.hpp>
#include <cmath>
#include <iostream>
#include <poet/active_function.hpp>
#include <vector>

double add(double a, double b)
{
	return a + b;
}

double multiply(double a, double b)
{
	return a * b;
}

double square_root(double a)
{
	return std::sqrt(a);
}

int main()
{
	static const unsigned num_vectors = 10;

	// input data, assigned some arbitrary values
	std::vector<boost::array<double, 2> > input_vectors;
	unsigned i;
	for(i = 0; i < num_vectors; ++i)
	{
		boost::array<double, 2> vec_2d;
		vec_2d.at(0) = i % 4;
		vec_2d.at(1) = i % 3;
		input_vectors.push_back(vec_2d);
	}

	// active_functions
	poet::active_function<double (double, double)> active_adder(&add);
	poet::active_function<double (double, double)> active_multiplier0(&multiply);
	poet::active_function<double (double, double)> active_multiplier1(&multiply);
	poet::active_function<double (double)> active_sqrt(&square_root);

	std::vector<poet::future<double> > lengths;
	// pass input data through active_function pipeline
	for(i = 0; i < input_vectors.size(); ++i)
	{
		poet::future<double> product0 = active_multiplier0(input_vectors.at(i).at(0), input_vectors.at(i).at(0));
		poet::future<double> product1 = active_multiplier1(input_vectors.at(i).at(1), input_vectors.at(i).at(1));

		poet::future<double> sum = active_adder(product0, product1);

		poet::future<double> root = active_sqrt(sum);

		lengths.push_back(root);
	}

	// wait for futures to become ready and convert them to values
	for(i = 0; i < lengths.size(); ++i)
	{
		double value = lengths.at(i);
		std::cout << "vector " << i << " = {" << input_vectors.at(i).at(0) << ", " <<
			input_vectors.at(i).at(1) << "}, length = " << value << "\n";
	}

	return 0;
}
