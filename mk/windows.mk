# Windows (MSYS2 UCRT64). Native toolchain only — never /usr/bin/g++.
CXX := /ucrt64/bin/g++
EXE := .exe

FLTK_CXXFLAGS := $(shell fltk-config --use-images --cxxflags)
FLTK_LDFLAGS  := $(shell fltk-config --use-images --ldflags)

CURL_CFLAGS := $(shell curl-config --cflags)
CURL_LIBS   := $(shell curl-config --libs)

CXXFLAGS += $(FLTK_CXXFLAGS) $(CURL_CFLAGS)
LDFLAGS  += -mwindows -static-libgcc -static-libstdc++
LDLIBS   += $(FLTK_LDFLAGS) $(CURL_LIBS) -lshell32 -lole32 -lcomctl32

PLATFORM_SRCS := src/platform/tray_win.cpp src/platform/notify_win.cpp
