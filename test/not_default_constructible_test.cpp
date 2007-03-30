// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <poet/future.hpp>
#include <iostream>

class not_def_con
{
public:
	not_def_con(int)
	{}
	not_def_con(const not_def_con&)
	{}
};

int main()
{
	poet::promise<not_def_con> prom;
	poet::future<not_def_con> fut(prom);
	BOOST_ASSERT(fut.ready() == false);
	prom.fulfill(not_def_con(0));
	BOOST_ASSERT(fut.ready());
	return 0;
}
