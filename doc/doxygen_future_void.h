/* This header exists purely for the sake of doxygen. */

namespace poet
{
	/*! \brief A void specialization of the future template class

	future<void> is for futures with no value.  For example, it may be used to
	wait on the completion of an asynchronous function which has no return value.
	In addition,
	a future&lt;void&gt; can assigned or constructed from a future&lt;T&gt; where T
	is any type.  This allows a future&lt;void&gt; to be used by code which is
	only interested in whether the future is ready or has an exception, and
	is not interested in the future's specific value or template type.
	*/
	template <>
	class future<void>
	{
	public:
		/*! void
		*/
		typedef void value_type;
		/*! boost::signal<void ()>::slot_type
		*/
		typedef boost::signal<void ()>::slot_type update_slot_type;

		/*! Same as the corresponding constructor for an unspecialized future\<T\>.
		*/
		future(const promise<void> &promise_in): base_type(reinterpret_cast<const promise<int> &>(promise_in))
		{}
		/*! A future<void> can be constructed from any type of promise.
		*/
		template <typename OtherType>
		future(const promise<OtherType> &promise)
		{
			future<OtherType> other_future(promise);
			*this = other_future;
		}
		/*! A future<void> can be constructed from any type of future.
		*/
		template <typename OtherType> future(const future<OtherType> &other)
		{
			BOOST_ASSERT(typeid(void) != typeid(OtherType));
			if(other._future_body == 0)
			{
				_future_body.reset();
				return;
			}
			boost::function<int (const OtherType&)> typedConversionFunction =
				boost::bind(&detail::null_conversion_function<OtherType>, _1);
			_future_body.reset(new detail::future_body_proxy<int, OtherType>(
				other._future_body, typedConversionFunction));
		}
		/*! Same as the corresponding function for an unspecialized future\<T\>.
		*/
		future()
		{}
		/*! Virtual destructor.
		*/
		virtual ~future() {}
		/*!
		A future&lt;void&gt; has no value, but get() can still be used to block until the
		future's promise is fulfilled or reneged.

		\exception cancelled_future if the future was cancelled.

		\exception unspecified if the future's promise is broken, get() will throw whatever exception
		was specified by the promise::renege() call.
		*/
		void get() const
		{
			if(_future_body == 0)
			{
				throw uncertain_future();
			}
			_future_body->getValue();
		}
		/*! The conversion operator has
		the same effects as the explicit get() function. */
		operator void () const
		{
			get();
		}
		/*! A future<void> can be assigned any type of future.
		*/
		template <typename OtherType> const future<void>& operator =(const future<OtherType> &other)
		{
			BOOST_ASSERT(typeid(void) != typeid(OtherType));
			_future_body.reset(new detail::future_body_proxy<void, OtherType>(other._future_body));
			return *this;
		}
		/*! Same as the corresponding function for an unspecialized future\<T\>.
		*/
		inline bool timed_join(const boost::xtime &absolute_time) const;
		/*! Same as the corresponding function for an unspecialized future\<T\>.
		*/
		inline bool ready() const;
		/*! Same as the corresponding function for an unspecialized future\<T\>.
		*/
		inline boost::signalslib::connection connect_update(const update_slot_type &slot) const;
		/*! Same as the corresponding function for an unspecialized future\<T\>.
		*/
		inline void cancel();
		/*! Same as the corresponding function for an unspecialized future\<T\>.
		*/
		inline bool has_exception() const;
	};

	/*! \brief A void specialization of the promise template class

	promise<void> may be used to create future<void> objects which have no value.
	In addition,
	a promise&lt;void&gt; can be constructed from a promise&lt;T&gt; where T
	is any type.  This allows a promise&lt;void&gt; to be used by code which
	only needs to be able to renege on a promise and not fulfill it.
	*/
	template<>
	class promise<void>
	{
	public:
		/*! void */
		typedef void value_type;

		/*! Same as the corresponding constructor for an unspecialized promise\<T\>.
		*/
		promise()
		{}
		/*! Same as the corresponding constructor for an unspecialized promise\<T\>.
		*/
		promise(const promise<void> &other): promise<int>(other)
		{}
		/*! A promise\<void\> may be constructed from a promise with any template type,
		although it cannot fulfill() such a promise.  It can renege on the promise,
		however. */
		template <typename OtherType>
		promise(const promise<OtherType> &other)
		{
			boost::function<int (const OtherType&)> conversion_function =
				boost::bind(&detail::null_conversion_function<OtherType>, _1);
			_pimpl->_future_body.reset(new detail::future_body_proxy<int, OtherType>(
				other._pimpl->_future_body, conversion_function));
		}
		/*! Same as the corresponding function for an unspecialized promise\<T\>.
		*/
		inline void fulfill(const future<void> &future_value);
		/*! Will make any future<void> objects referencing this promise become ready.
		\throws std::invalid_argument if this promise<void> actually references a promise
		with a non-void template type.
		*/
		void fulfill()
		{
			base_type::fulfill(0);
		}
		/*! Same as the corresponding function for an unspecialized promise\<T\>.
		*/
		template <typename E>
		void renege(const E &exception)
		{
			base_type::renege(exception);
		}
		/*! Same as the corresponding function for an unspecialized promise\<T\>.
		*/
		void renege(const poet::exception_ptr &exp)
		{
			base_type::renege(exp);
		}
	};
}
