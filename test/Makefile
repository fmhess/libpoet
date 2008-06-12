THREAD_SAFE_SIGNALS_DIR=/home/fhess/svn/thread_safe_signals
BOOST_INC_DIR=/usr/local/boost_r45175/include/boost-1_35
BOOST_LIB_DIR=/usr/local/lib
CXX=g++
#set CC since it is used for linking .o files by built-in rule
CC=$(CXX)
RM=rm -f
LD=g++

PROGRAMS = active_function_test active_object_test acyclic_mutex_test \
	acyclic_shared_mutex_test acyclic_mutex_upgrade_lock_test \
	exception_test future_combining_barrier_test future_selector_test \
	future_test future_waits_test future_void_test \
	lazy_future_test lock_move_test \
	monitor_test new_mutex_api_test \
	not_default_constructible_test promise_count_test timed_join_test undead_active_function_test

CPPFLAGS= -pthread -I.. -I$(THREAD_SAFE_SIGNALS_DIR) -I$(BOOST_INC_DIR)
CXXFLAGS= -O0 -g -Wall
LDFLAGS= -L$(BOOST_LIB_DIR) -lboost_thread-gcc41-mt -pthread
.PHONY: all clean dep test valgrind_test

all: $(PROGRAMS)

clean:
	$(RM) $(PROGRAMS)

test: $(PROGRAMS)
	(export LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$(BOOST_LIB_DIR); for test_prog in $(PROGRAMS); do ./$$test_prog; done;)

valgrind_test: $(PROGRAMS)
	(export LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$(BOOST_LIB_DIR); for test_prog in $(PROGRAMS); do valgrind ./$$test_prog; done;)

