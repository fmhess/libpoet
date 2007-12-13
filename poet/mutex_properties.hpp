/*
	A template class for getting properties of a mutex.  Covers mutex types from
	boost.thread and libpoet.  Other mutex classes can provide their own
	specializations.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-12-07
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MUTEX_PROPERTIES_HPP
#define _POET_MUTEX_PROPERTIES_HPP

namespace boost
{
	// forward declarations
	class mutex;
	class try_mutex;
	class timed_mutex;
	class recursive_mutex;
	class recursive_try_mutex;
	class recursive_timed_mutex;
};

namespace poet
{
	enum mutex_model
	{
		mutex_concept,
		try_mutex_concept,
		timed_mutex_concept
	};

	// forward declarations
	template<typename Mutex, typename Key, typename KeyCompare> class acyclic_mutex;
	template<typename T, typename Mutex> class monitor;
	template<typename T, typename Mutex> class monitor_ptr;
	namespace detail
	{
		template<typename Mutex, bool recursive, enum mutex_model model, typename Key, typename KeyCompare>
		class specialized_acyclic_mutex;
	};

	template<typename Mutex> class mutex_properties
	{};

	template<>
	class mutex_properties<boost::mutex>
	{
	public:
		static const bool recursive = false;
		static const mutex_model model = mutex_concept;
	};

	template<>
	class mutex_properties<boost::try_mutex>
	{
	public:
		static const bool recursive = false;
		static const mutex_model model = try_mutex_concept;
	};

	template<>
	class mutex_properties<boost::timed_mutex>
	{
	public:
		static const bool recursive = false;
		static const mutex_model model = timed_mutex_concept;
	};

	template<>
	class mutex_properties<boost::recursive_mutex>
	{
	public:
		static const bool recursive = true;
		static const mutex_model model = mutex_concept;
	};

	template<>
	class mutex_properties<boost::recursive_try_mutex>
	{
	public:
		static const bool recursive = true;
		static const mutex_model model = try_mutex_concept;
	};

	template<>
	class mutex_properties<boost::recursive_timed_mutex>
	{
	public:
		static const bool recursive = true;
		static const mutex_model model = timed_mutex_concept;
	};

	template<typename Mutex, typename Key, typename KeyCompare>
	class mutex_properties<poet::acyclic_mutex<Mutex, Key, KeyCompare> >:
		public mutex_properties<Mutex>
	{};

	template<typename T, typename Mutex>
	class mutex_properties<poet::monitor<T, Mutex> >:
		public mutex_properties<Mutex>
	{};

	template<typename T, typename Mutex>
	class mutex_properties<poet::monitor_ptr<T, Mutex> >:
		public mutex_properties<Mutex>
	{};

	template<typename Mutex, bool recursive, enum mutex_model model, typename Key, typename KeyCompare>
	class mutex_properties<detail::specialized_acyclic_mutex<Mutex, recursive, model, Key, KeyCompare> >:
		public mutex_properties<Mutex>
	{};
};

#endif // _POET_MUTEX_PROPERTIES_HPP
