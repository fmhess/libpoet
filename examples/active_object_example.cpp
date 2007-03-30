// An example showing how a full active object class can be built
// from a servant class, a scheduler, and active_functions.
// The "chunkifier" is a queue that takes single elements as inputs
// and outputs chunks of N elements accumulated in a std::vector.
// The elements in the output chunks are added to a chunk as
// they became ready, not necessarily in the order their futures
// were added to the chunkifier.

// Copyright (C) Frank Mori Hess 2007
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <poet/active_function.hpp>
#include <deque>
#include <vector>

// servant class
template <typename T>
class passive_chunkifier
{
public:
	passive_chunkifier(unsigned chunk_size): _chunk_size(chunk_size)
	{}
	void add_element(const T& value)
	{
		_deque.push_back(value);
	}
	std::vector<T> get_chunk()
	{
		if(_deque.size() < _chunk_size)
		{
			throw std::invalid_argument("Not enough elements to create chunk.");
		}
		std::vector<T> chunk;
		unsigned i;
		for(i = 0; i < _chunk_size; ++i)
		{
			chunk.push_back(_deque.front());
			_deque.pop_front();
		}
		return chunk;
	}
	/* guard function to prevent scheduler from running get_chunk unless
	there are enough elements available to make a chunk */
	bool get_chunk_guard() const
	{
		return _deque.size() >= _chunk_size;
	}
private:

	std::deque<T> _deque;
	const unsigned _chunk_size;
};

// chunkifying active object class
template <typename T>
class active_chunkifier
{
private:
	boost::shared_ptr<passive_chunkifier<T> > _servant;
	boost::shared_ptr<typename poet::scheduler> _scheduler;
public:
	typedef typename poet::active_function<void (T)> add_element_type;
	typedef typename poet::active_function<std::vector<T> ()> get_chunk_type;

	active_chunkifier(unsigned chunk_size):
		_servant(new passive_chunkifier<T>(chunk_size)),
		_scheduler(new poet::scheduler),
		add_element(typename add_element_type::passive_slot_type(&passive_chunkifier<T>::add_element, _servant, _1),
			0, _scheduler),
		get_chunk(typename get_chunk_type::passive_slot_type(&passive_chunkifier<T>::get_chunk, _servant),
			boost::bind(&passive_chunkifier<T>::get_chunk_guard, _servant),
			_scheduler)
	{}
	add_element_type add_element;
	get_chunk_type get_chunk;
};

int main()
{
	static const unsigned chunk_size = 10;
	static const unsigned num_chunks = 2;
	unsigned i;

	active_chunkifier<int> chunky(chunk_size);

	// get future chunks
	std::vector<poet::future<std::vector<int> > > future_chunks;
	for(i = 0; i < num_chunks; ++i)
	{
		future_chunks.push_back(chunky.get_chunk());
	}

	// push elements into chunkifier
	for(i = 0; i < num_chunks * chunk_size; ++i)
	{
		chunky.add_element(i);
	}

	// wait for future chunks to become ready and dump them
	for(i = 0; i < future_chunks.size(); ++i)
	{
		std::cout << "chunk number " << i << "\n";
		std::vector<int> value = future_chunks.at(i);
		unsigned j;
		for(j = 0; j < value.size(); ++j)
		{
			std::cout << "\t" << j << " element = " << value.at(j) << "\n";
		}
		std::cout << std::endl;
	}

	return 0;
}
