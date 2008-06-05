#include <boost/thread.hpp>
#include <poet/future_barrier.hpp>
#include <poet/future_select.hpp>
#include <cassert>
#include <iostream>
#include <vector>

void preready_push_test()
{
	poet::future_selector<int> selector;
	poet::future<int> selected_future = selector.selected();
	selector.pop_selected();
	poet::future<int> preready_future = 1;
	selector.push(preready_future);
	assert(selected_future.ready());
}

int main()
{
	std::cerr << __FILE__ << "... ";

	{
		poet::future_selector<int> selector;

		std::vector<poet::promise<int> > promises;
		promises.push_back(poet::promise<int>());
		promises.push_back(poet::promise<int>());
		promises.push_back(poet::promise<int>());

		unsigned i;
		for(i = 0; i < promises.size(); ++i)
		{
			selector.push(promises.at(i));
		}
		std::vector<poet::future<int> > selected_results;
		for(i = 0; i < promises.size(); ++i)
		{
			selected_results.push_back(selector.selected());
			selector.pop_selected();
		}
		for(i = 0; i < promises.size(); ++i)
		{
			selected_results.push_back(selector.selected());
			selector.pop_selected();
		}
		for(i = 0; i < selected_results.size(); ++i)
		{
			assert(selected_results.at(i).ready() == false);
			assert(selected_results.at(i).has_exception() == false);
		}

		assert(selected_results.at(0).ready() == false);
		promises.at(1).fulfill(1);
		assert(selected_results.at(0).ready() == true);
		assert(selected_results.at(0).get() == 1);

		assert(selected_results.at(1).ready() == false);
		promises.at(0).fulfill(0);
		assert(selected_results.at(1).ready() == true);
		assert(selected_results.at(1).get() == 0);

		assert(selected_results.at(2).ready() == false);
		promises.at(2).fulfill(2);
		assert(selected_results.at(2).ready() == true);
		assert(selected_results.at(2).get() == 2);
	}

	preready_push_test();

	std::cerr << "OK" << std::endl;
	return 0;
}
