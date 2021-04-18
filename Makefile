#
# Makefile
#
CXX = g++

TARGET = infreqv6

CXXFLAGS = -O2 -g0 -Wall -std=c++14

LIBS = 

LDFLAGS =

OBJS = client.o

.SUFFIXES: .o .cpp
.PHONY: all clean

all: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
.cpp.o:
	$(CXX) $(CXXFLAGS) -c $<

