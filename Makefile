CC := gcc
CXX := g++
.DEFAULT_GOAL := all
DEST=~/.local
########## for frontend dev BEGIN ###########################

module_list := wfmd-efvi-hw-czce-receive-dce-mc.so
efvi-udp.o : efvi-udp.c
	$(CC) -c -o $@ $^ -fPIC
wfmd-efvi-hw-czce-receive-dce-mc.so: wfmd-efvi-hw-czce-receive-dce-mc.o efvi-udp.o
	$(CXX) -shared -o $@ $^ -lciul1

CFLAGS := -Wall -I $(DEST)/include -fPIC -O2 -g -pthread -lonload -lciul1
CXXFLAGS := $(CFLAGS) --std=c++11

all: $(module_list)

install: $(module_list)
	install -d $(DEST)/lib
	install -p $(module_list) $(DEST)/lib

clean:
	$(RM) $(module_list) *.o

.PHONY: all install clean
