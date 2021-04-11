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

ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

.SUFFIXES: .o .cpp
.PHONY: all clean install

all: $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

$(INFREQV6): $(OBJS_INFREQV6)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS_INFREQV6) $(LIBS)
.cpp.o:
	$(CXX) $(CXXFLAGS) -c $<

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/
