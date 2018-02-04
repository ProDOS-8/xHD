xHDServer: main.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

LDLIBS = -lserialport

ifeq ($(OS),Windows_NT)

LDLIBS += -lcfgmgr32 -lsetupapi

CC  = gcc
CXX = g++
CPPFLAGS = -m64 -I win32
LDFLAGS  = -m64 -L win32

endif # Windows_NT
