/*
	By deriving from monitor_base, an object can release the
	mutex of the monitor_ptr which owns it and wait to be
	notified.

	begin: Frank Hess <fmhess@users.sourceforge.net>  2007-08-27
*/

//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef _POET_MONITOR_BASE_HPP
#define _POET_MONITOR_BASE_HPP

#include <boost/function.hpp>
#include <poet/detail/monitor_synchronizer.hpp>
#include <poet/mutex_properties.hpp>

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
		/* Does nothing except prevent _syncer from being disturbed
		on assignment by default assignment operator.  It's fine if derived
		class overrides this and doesn't call it, since it doesn't do anything! */
		monitor_base& operator=(const monitor_base &rhs)
		{
			return *this;
		}
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

#include <poet/detail/monitor_base.ipp>

#endif // _POET_MONITOR_BASE_DECL_HPP
