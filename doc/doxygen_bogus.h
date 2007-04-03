/* This header exists purely for the sake of doxygen. */

/*!
	\class std::runtime_error
	\brief std::runtime_error
*/
/*!
	\class boost::postconstructible
	\brief boost::postconstructible

	See the documentation for
	<a href="http://www.comedi.org/projects/thread_safe_signals/boostbook/boost/postconstructible.html">postconstructible in thread_safe_signals</a>
	for more information.

*/
/*! \brief boost namespace

See the
<a href="http://www.comedi.org/projects/thread_safe_signals/doxygen/index.html">thread_safe_signals</a>
and
<a href="http://www.boost.org">boost</a>
documentation for more information.
*/
namespace boost
{
/*! \overload
*/
  template<typename T, typename D>
  shared_ptr<T> deconstruct_ptr(T *ptr, D deleter);
/*!
	See the documentation for
	<a href="http://www.comedi.org/projects/thread_safe_signals/boostbook/boost/deconstruct_ptr.html">deconstruct_ptr in thread_safe_signals</a>
	for more information.
*/
	template<typename T>
	shared_ptr<T> deconstruct_ptr(T *ptr);
/*! \brief boost::enable_shared_from_this

	See boost documentation for more information.
*/
	template<typename T> class enable_shared_from_this {};

/*!\brief boost::slot

See the documentation for the
<a href="http://www.comedi.org/projects/thread_safe_signals/boostbook/boost/slot.html">slot class in thread_safe_signals</a>
for more information.
*/
	template<typename Signature, typename SlotFunction = boost::function<Signature> >
	class slot
	{};
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
		typedef typename boost::function_traits<Signature>::result_type passive_result_type;
		/*! future<R> */
		typedef poet::future<passive_result_type> result_type;
		/*! Slot type for the passive function the active_function is constructed from. */
		typedef boost::slot<Signature> passive_slot_type;

		/*! \param passive_function The underlying function this active_function object will call.  The
		boost::slot class supports tracking of arbitrary boost::shared_ptr which are associated
		with the slot.  For example, if the slot is constructed from a non-static member function, the lifetime
		of the member function's object can be tracked and the slot prevented from running after
		the object is destroyed.
		\param guard The active_function's scheduler will not execute a method request until
			the guard returns true and all the input futures are ready.  By default, the guard will always return true.
		\param scheduler_ptr Specify a scheduler object for the active_function to post its method requests to.
		By default, a new Scheduler object is created for the active_function.  If the active_function is
		providing a method as part of an active object class, you may wish for all the class' methods
		to share the same scheduler.
		*/
		active_function(const passive_slot_type &passive_function,
			const boost::function<bool ()> &guard = 0,
			boost::shared_ptr<scheduler_base> scheduler_ptr = boost::shared_ptr<scheduler_base>())
		{}
		/*! Virtual destructor. */
		virtual ~active_function() {}
		/*! Const overload. */
		future<R> operator()(future<T1> arg1, future<T2> arg2, ..., future<TN> argN) const;
		/*! Invocation creates a method request and sends it to the active_function's scheduler.
			The method request may be cancelled by calling future::cancel() on the returned
			future.

			Note the active_function takes futures as arguments, as well as returning a future.  This
			allows future results to be passed from one active_function to another without waiting
			for the result to become ready.  Since futures are constructible from their
			value types, the active_function can also take ordinary values not wrapped
			futures as arguments.
		 */
		future<R> operator()(future<T1> arg1, future<T2> arg2, ..., future<TN> argN);
		/*! Calls scheduler_base::wake() on the active_function's scheduler. */
		void wake();
		/*! Calls the boost::slot::expired() query method on the slot this active_function was constructed
		from. */
		bool expired() const;
	};
}

/*!
	\example pipeline.cpp
*/

/*!
	\example active_object_example.cpp
*/
