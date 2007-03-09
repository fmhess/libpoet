/*
	A test program for futures and active object classes

	Author: Frank Hess <frank.hess@nist.gov>
	Begin: 2005-11
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

#include <boost/bind.hpp>
#include <poet/active_function.hpp>
#include <iostream>
#include <vector>
#include <unistd.h>

int increment(int value)
{
	// sleep for a bit to simulate doing something nontrivial
	sleep(1);
	return ++value;
}

bool myGuard()
{
	return true;
}
int main()
{
	// the bind() is only for illustration and isn't actually needed in this case,
	// since the signatures match exactly.
	poet::active_function<int (int)> inc1(boost::bind(&increment, _1));
	poet::active_function<int (int)> inc2(&increment, &myGuard);
	std::vector<poet::future<int> > results(2);
	unsigned i;
	std::cerr << "getting Futures..." << std::endl;
	for(i = 0; i < results.size(); ++i)
	{
		poet::future<unsigned> temp = inc1(i);
		std::cerr << "passing result of inc1 to inc2..." << std::endl;
		results.at(i) = inc2(temp);
		std::cerr << "stored future results[" << i << "]" << std::endl;
	}
	std::cerr << "converting Futures to values..." << std::endl;
	for(i = 0; i < results.size(); ++i)
	{
		int value = results.at(i);
		std::cerr << "value from results[" << i << "] is " << value << std::endl;
	}
	return 0;
}
