#include <poet/monitor.hpp>
#include <poet/monitor_locks.hpp>
#include <cassert>

poet::monitor_unique_lock<poet::monitor<int> > get_lock(poet::monitor<int> & m)
{
    return poet::monitor_unique_lock<poet::monitor<int> >(m);
}

void test_unique_lock_move_from_rvalue_on_construction()
{
    poet::monitor<int> m;
    poet::monitor_unique_lock<poet::monitor<int> > l(get_lock(m));
    assert(l.owns_lock());
    assert(l.mutex()==&m);
}

int main()
{
	test_unique_lock_move_from_rvalue_on_construction();
	return 0;
}
