#include <boost/tuple/tuple.hpp>
#include <boost/variant/variant.hpp>
#include <poet/future_barrier.hpp>
#include <poet/future_select.hpp>

template<typename T1, typename T2>
boost::tuple<T1, T2> create_tuple(const T1 &a1, const T2 &a2)
{
	return boost::tuple<T1, T2>(a1, a2);
}

template<typename T1, typename T2>
poet::future<boost::tuple<T1, T2> > operator&&(const poet::future<T1> &f1, const poet::future<T2> &f2)
{
	return poet::future_combining_barrier<boost::tuple<T1, T2> >(&create_tuple<T1, T2>, f1, f2);
}

struct identity
{
	template<typename T>
	const T& operator()(const T &t)
	{
		return t;
	}
};

template<typename T1, typename T2>
poet::future<boost::variant<T1, T2> > operator||(const poet::future<T1> &f1, const poet::future<T2> &f2)
{
	typedef boost::variant<T1, T2> result_type;
	poet::future<result_type> variant_f1 = poet::future_combining_barrier<result_type>(identity(), f1);
	poet::future<result_type> variant_f2 = poet::future_combining_barrier<result_type>(identity(), f2);
	return poet::future_select(variant_f1, variant_f2);
}

#include <cassert>
#include <iostream>
#include <string>
#include <boost/variant.hpp>

int main()
{
	std::cerr << __FILE__ << "... ";

	poet::promise<int> pi;
	poet::promise<double> pd;
	poet::promise<std::string> pstr;
	poet::promise<std::string> pstr2;
	poet::future<int> fi = pi;
	poet::future<double> fd = pd;
	poet::future<std::string> fstr = pstr;
	poet::future<std::string> fstr2 = pstr2;

	poet::future<boost::tuple<int, double> > fall = fi && fd;
	poet::future<boost::variant<boost::tuple<int, double>, std::string > > fany = fall || fstr;
	poet::future<boost::variant<boost::tuple<int, double>, std::string > > fany2 = fall || fstr2;

	assert(fall.ready() == false);
	assert(fany.ready() == false);
	assert(fany2.ready() == false);

	pi.fulfill(1);
	assert(fall.ready() == false);

	assert(fany.ready() == false);
	assert(fstr.ready() == false);
	pstr.fulfill("hello");
	assert(fany.ready() == true);
	assert(fstr.ready() == true);

	assert(fall.ready() == false);
	assert(fany2.ready() == false);
	pd.fulfill(1.5);
	assert(fall.ready() == true);
	assert(fany2.ready() == true);

	assert(fall.get().get<0>() == 1);
	assert(std::abs(fall.get().get<1>() / 1.5 - 1.0) < 1e-6);
	assert(boost::get<std::string>(fany.get()) == "hello");
	boost::tuple<int, double> fany2_value = boost::get<boost::tuple<int, double> >(fany2.get());
	assert(fany2_value.get<0>() == 1);

	std::cerr << "OK" << std::endl;
	return 0;
}
