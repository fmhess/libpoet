/* This header exists purely for the sake of doxygen. */

/*!
	\class std::runtime_error
	\brief std::runtime_error
*/
/*!
	\class boost::postconstructible
	\brief boost::postconstructible

	See thread_safe_signals documentation for more information.
*/
/*! \brief boost namespace

See boost documentation for more information.
*/
namespace boost
{
/*! \overload
*/
  template<typename T, typename D>
  shared_ptr<T> deconstruct_ptr(T *ptr, D deleter);
/*!
	See thread_safe_signals documentation for more information.
*/
	template<typename T>
	shared_ptr<T> deconstruct_ptr(T *ptr);
/*! \brief boost::enable_shared_from_this

	See boost documentation for more information.
*/
	template<typename T> class enable_shared_from_this {};
}

/*! \brief libpoet namespace
*/
namespace poet
{
	/* You must EXCLUDE the real declaration of active_function in the Doxyfile doxygen config file for
	the following to work. */
	/*!\brief Create an active object from an ordinary function or object.

	In the following, the active_function is taken to have a signature of:
\code
active_function<R (T1, T2, ..., TN)>
\endcode
	*/
	template<typename Signature>
	class active_function: public poet::detail::active_functionN<boost::function_traits<Signature>::arity, Signature>::type
	{
	public:
		/*! R */
		typedef typename boost::function_traits<Signature>::result_type bare_result_type;
		/*! future<R> */
		typedef poet::future<bare_result_type> result_type;
		/*! \param passiveFunction The underlying function this active_function object will call.  This
			parameter will probably be combined with tracking provided by the <em>servant</em> parameter as a single
			boost::slot argument in the future.
		\param guard The active_function's scheduler will not execute a method request until
			the guard returns true.  By default, the guard will always return true.
		\param scheduler Specify a scheduler object for the active_function to post its method requests to.
		By default, a new Scheduler object is created for the active_function.  If the active_function is
		providing a method as part of a full active object class, you may wish for all the class' methods
		to share the same scheduler.
		\param servant Provides tracking of a servant object's lifetime, if <em>servant</em> is non-null.
			The <em>servant</em> is not null, it will
			be stored as a boost::weak_ptr.  Whenever operator()() is called, the <em>servant</em>
			will be converted back into a boost::shared_ptr which will exist until the corresponding
			method request is dispatched by the scheduler.  If the weak_ptr has already expired, then
			operator()() will throw an exception.  This
			parameter will probably be combined with the <em>passiveFunction</em> parameter as a single
			boost::slot argument in the future.
		*/
		active_function(const boost::function<Signature> &passiveFunction,
			const boost::function<bool ()> &guard = 0,
			boost::shared_ptr<scheduler_base> scheduler = boost::shared_ptr<scheduler_base>(),
			boost::shared_ptr<void> servant = boost::shared_ptr<void>()):
			base_type(passiveFunction, guard, scheduler, servant)
		{}
		/*! Virtual destructor. */
		virtual ~active_function() {}
		/*! Const overload. */
		future<R> operator()(future<T1> arg1, future<T2> arg2, ..., future<TN> argN) const;
		/*! Invocation creates a method request and sends it to the active_function's scheduler.
			The method request may be cancelled by calling future::cancel() on the returned
			future.

			Note the active_function takes Futures as arguments, as well as returning a future.  This
			allows future results to be passed from one active_function to another without waiting
			for the result to become ready.  Since Futures are constructible from their
			value types, the active_function will also take values not wrapped in a future as arguments.
		 */
		future<R> operator()(future<T1> arg1, future<T2> arg2, ..., future<TN> argN);
		/*! Calls Scheduler::wake() on the active_function's scheduler. */
		void wake();
	};
}

/*!
	\example active_function_test.cpp
*/
