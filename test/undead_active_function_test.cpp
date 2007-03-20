// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/assert.hpp>
#include <iostream>
#include <poet/active_function.hpp>
#include <unistd.h>
#include <vector>

int passive_function()
{
	std::cerr << __FUNCTION__ << ": going to sleep." << std::endl;
	sleep(1);
	std::cerr << __FUNCTION__ << ": returning result." << std::endl;
	return 1;
}

int main()
{
	std::vector<poet::future<int> > results;
	{
		poet::active_function<int ()> short_lived(&passive_function);
		results.push_back(short_lived());
		results.push_back(short_lived());
	}
	std::cerr << "active function has been destroyed." << std::endl;
	unsigned i;
	for(i = 0; i < results.size(); ++i)
	{
		results.at(i).get();
	}
	return 0;
}
