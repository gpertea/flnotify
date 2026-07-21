# macOS — NSStatusItem tray + NSUserNotification, Objective-C++ (.mm).
# TODO (port phase): fill in once tray_mac/notify_mac exist.
CXX ?= clang++

FLTK_CXXFLAGS := $(shell fltk-config --use-images --cxxflags)
FLTK_LDFLAGS  := $(shell fltk-config --use-images --ldflags)

CXXFLAGS += $(FLTK_CXXFLAGS)
LDFLAGS  += -framework Cocoa
LDLIBS   += $(FLTK_LDFLAGS)

PLATFORM_SRCS := src/platform/tray_mac.mm src/platform/notify_mac.mm

$(BUILD)/%.o: src/%.mm
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -ObjC++ -MMD -MP -c $< -o $@
