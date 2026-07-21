# flnotify — GNU Makefile (no CMake by project decision).
# Platform picked via uname -s; per-platform bits live in mk/<platform>.mk.

UNAME_S := $(shell uname -s)
ifneq (,$(findstring _NT,$(UNAME_S)))
  PLATFORM := windows
else ifeq ($(UNAME_S),Linux)
  PLATFORM := linux
else ifeq ($(UNAME_S),Darwin)
  PLATFORM := macos
else
  $(error Unsupported platform: $(UNAME_S))
endif

BUILD  := build/$(PLATFORM)
TARGET := flnotify

CXXFLAGS ?= -O2
CXXFLAGS += -std=c++20 -Wall -Wextra -Isrc -Ithird_party
LDFLAGS  :=
LDLIBS   :=
EXE      :=
PLATFORM_SRCS :=

include mk/$(PLATFORM).mk

SRCS := $(wildcard src/core/*.cpp) $(wildcard src/ui/*.cpp) $(wildcard src/net/*.cpp) \
        $(PLATFORM_SRCS)
OBJS := $(patsubst src/%.cpp,$(BUILD)/%.o,$(filter %.cpp,$(SRCS))) \
        $(patsubst src/%.mm,$(BUILD)/%.o,$(filter %.mm,$(SRCS)))
DEPS := $(OBJS:.o=.d)

BIN := $(TARGET)$(EXE)

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

$(BUILD)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf build $(TARGET) $(TARGET).exe compile_commands.json

# Hand-emitted compile database for clangd (one entry per source).
# On Windows clangd needs Windows-style paths, so translate MSYS paths via cygpath.
compile_commands.json: Makefile mk/$(PLATFORM).mk
	@DIR=$$(cygpath -m '$(CURDIR)' 2>/dev/null || echo '$(CURDIR)'); \
	UCRT=$$(cygpath -m /ucrt64 2>/dev/null || echo /ucrt64); \
	CMD=$$(echo '$(CXX) $(CXXFLAGS)' | sed "s|/ucrt64|$$UCRT|g"); \
	{ echo '['; \
	sep=''; \
	for s in $(SRCS); do \
	  printf '%b  {"directory": "%s", "command": "%s -c %s", "file": "%s"}' \
	    "$$sep" "$$DIR" "$$CMD" "$$s" "$$s"; \
	  sep=',\n'; \
	done; \
	printf '\n]\n'; } > $@

.PHONY: all run clean
