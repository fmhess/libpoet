THREAD_SAFE_SIGNALS_DIR=/home/fhess/svn/thread_safe_signals

CXX=g++
#set CC since it is used for linking .o files by built-in rule
CC=$(CXX)
RM=rm -f
LD=g++

PROGRAMS = active_function_test active_object_test acyclic_mutex_test \
	exception_test future_test future_void_test monitor_test \
	not_default_constructible_test promise_count_test timed_join_test undead_active_function_test

CPPFLAGS= -pthread -I.. -I$(THREAD_SAFE_SIGNALS_DIR)
CXXFLAGS= -O0 -g -Wall
LDFLAGS= -lboost_thread -pthread
.PHONY: all clean dep

all: $(PROGRAMS)

clean:
	$(RM) $(PROGRAMS)

test:
	for test_prog in $(PROGRAMS); do ./$$test_prog; done;

