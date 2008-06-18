// An toy example showing how a full active object class can be defined
// from a servant class, a scheduler, and active_functions.
// The active_functions all share the same scheduler, so the
// methods of the servant class are only called by a single
// scheduler thread.

// Copyright (C) Frank Mori Hess 2007-2008
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/shared_ptr.hpp>
#include <poet/active_function.hpp>
#include <iostream>

// servant class
template <typename T>
class passive_value
{
public:
	passive_value(const T& initial_value): _value(initial_value)
	{}
	const T& add(const T& x)
	{
		_value += x;
		return _value;
	}
	const T& multiply(const T& x)
	{
		_value *= x;
		return _value;
	}
	const T& get() const
	{
		return _value;
	}
private:
	T _value;
};

// active object class
template <typename T>
class active_value
{
private:
	boost::shared_ptr<passive_value<T> > _servant;
	boost::shared_ptr<poet::scheduler> _scheduler;
public:
	typedef typename poet::active_function<T (T)> add_type;
	typedef typename poet::active_function<T (T)> multiply_type;
	typedef typename poet::active_function<T ()> get_type;

	active_value(const T& initial_value):
		_servant(new passive_value<T>(initial_value)),
		_scheduler(new poet::scheduler),
		add(typename add_type::passive_slot_type(&passive_value<T>::add, _servant, _1),
			_scheduler),
		multiply(typename multiply_type::passive_slot_type(&passive_value<T>::multiply, _servant, _1),
			_scheduler),
		get(typename get_type::passive_slot_type(&passive_value<T>::get, _servant),
			_scheduler)
	{}
	add_type add;
	multiply_type multiply;
	get_type get;
};

int main()
{
	active_value<int> my_active_object(1);
	std::cout << "initial value is " << my_active_object.get().get() << std::endl;

	my_active_object.add(10);
	std::cout << "adding 10 and multiplying by 2 gives " << my_active_object.multiply(2).get() << std::endl;

	return 0;
}
