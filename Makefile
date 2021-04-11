#
# Makefile
#
CXX = g++

INFREQV6 = infreqv6
TARGET = $(INFREQV6)

CXXFLAGS = -O2 -g0 -Wall -std=c++14

LIBS = -lpthread

LDFLAGS =

OBJS = 
OBJS_INFREQV6 = client.o $(OBJS)

.SUFFIXES: .o .cpp
.PHONY: all clean

all: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

$(INFREQV6): $(OBJS_INFREQV6)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS_INFREQV6) $(LIBS)
.cpp.o:
	$(CXX) $(CXXFLAGS) -c $<
