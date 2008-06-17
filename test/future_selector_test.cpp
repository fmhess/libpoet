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
	assert(selector.size() == 0);
	selector.pop_selected();
	assert(selector.size() == -1);
	poet::future<int> preready_future = 1;
	selector.push(preready_future);
	assert(selected_future.ready());
	assert(selector.size() == 0);
}

void future_selector_scope_test()
{
	static const int test_value = 5;

	poet::promise<int> prom;
	poet::future<int> selected_future;
	poet::future<int> hopeless_selected_future;
	{
		poet::future_selector<int> selector;
		selected_future = selector.selected();
		selector.pop_selected();
		hopeless_selected_future = selector.selected();
		selector.push(prom);
		assert(hopeless_selected_future.has_exception() == false);
	}
	assert(hopeless_selected_future.has_exception() == true);
	assert(selected_future.has_exception() == false);

	assert(selected_future.ready() == false);
	prom.fulfill(test_value);
	assert(selected_future.ready() == true);
	assert(selected_future.get() == test_value);
}

void future_selector_copy_test()
{
	std::vector<poet::promise<int> > promises;
	promises.push_back(poet::promise<int>());
	promises.push_back(poet::promise<int>());
	promises.push_back(poet::promise<int>());

	poet::future_selector<int> selector;
	selector.push(promises.at(0));
	poet::future_selector<int> selector_copy;
	selector_copy = selector;
	selector.push(promises.at(1));
	selector_copy.push(promises.at(2));

	assert(selector.selected().ready() == false);
	assert(selector_copy.selected().ready() == false);
	promises.at(0).fulfill(0);
	assert(selector.selected().ready());
	assert(selector.selected().get() == 0);
	assert(selector_copy.selected().ready());
	assert(selector_copy.selected().get() == 0);

	selector.pop_selected();
	selector_copy.pop_selected();

	assert(selector.selected().ready() == false);
	assert(selector_copy.selected().ready() == false);
	promises.at(1).fulfill(1);
	assert(selector.selected().ready());
	assert(selector.selected().get() == 1);
	assert(selector_copy.selected().ready() == false);

	selector.pop_selected();

	assert(selector.selected().ready() == false);
	promises.at(2).fulfill(2);
	assert(selector_copy.selected().ready());
	assert(selector_copy.selected().get() == 2);
	assert(selector.selected().ready() == false);
}

std::string converter(int)
{
	return "hello, world!";
}

void future_selector_other_type_push_test()
{
	poet::promise<int> promise;
	poet::future_selector<std::string> selector;
	selector.push(&converter, poet::detail::identity(), poet::future<int>(promise));
	poet::future<std::string> future = selector.selected();
	assert(future.ready() == false);
	promise.fulfill(0);
	assert(future.ready());
	assert(future.get() == "hello, world!");
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
	future_selector_scope_test();
	future_selector_copy_test();
	future_selector_other_type_push_test();

	std::cerr << "OK" << std::endl;
	return 0;
}
