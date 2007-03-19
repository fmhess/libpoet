THREAD_SAFE_SIGNALS_DIR=/home/fhess/src/thread_safe_signals

CXX=g++
#CXX=/usr/local/gcc-4.1.2/bin/g++
#set CC since it is used for linking .o files by built-in rule
CC=$(CXX)
RM=rm -f
LD=g++

PROGRAMS = future_test active_object_test active_function_test

CPPFLAGS= -pthread -I.. -I$(THREAD_SAFE_SIGNALS_DIR)
CXXFLAGS= -O3 -Wall
LDFLAGS= -lboost_thread -pthread
.PHONY: all clean dep

all: $(PROGRAMS)

clean:
	$(RM) $(PROGRAMS)


