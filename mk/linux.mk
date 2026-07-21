# Linux — tray via libayatana-appindicator, notifications via libnotify.
# TODO (port phase): fill in pkg-config flags once tray_linux/notify_linux exist.
CXX ?= g++

FLTK_CXXFLAGS := $(shell fltk-config --use-images --cxxflags)
FLTK_LDFLAGS  := $(shell fltk-config --use-images --ldflags)

CXXFLAGS += $(FLTK_CXXFLAGS)
LDLIBS   += $(FLTK_LDFLAGS)

PLATFORM_SRCS := src/platform/tray_linux.cpp src/platform/notify_linux.cpp
