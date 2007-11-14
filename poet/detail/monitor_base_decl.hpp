/*
	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-08-27
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_BASE_DECL_HPP
#define _POET_MONITOR_BASE_DECL_HPP

#include <boost/function.hpp>
#include <poet/detail/monitor_synchronizer.hpp>

namespace poet
{
	template<typename T, typename Mutex>
	class monitor_ptr;
	namespace detail
	{
		class monitor_synchronizer_base;
	};

	class monitor_base
	{
	public:
	protected:
		inline void wait() const;
		template<typename Pred>
		void wait(Pred pred) const
		{
			_syncer->wait(pred);
		};
		inline void notify_one() const;
		inline void notify_all() const;
	private:
		template<typename T, typename Mutex>
		friend class monitor_ptr;

		inline void set_synchronizer(const boost::shared_ptr<detail::monitor_synchronizer_base> &syncer) const;

		mutable boost::shared_ptr<detail::monitor_synchronizer_base> _syncer;
	};
};

#endif // _POET_MONITOR_BASE_DECL_HPP