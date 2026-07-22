#pragma once
#include <functional>
#include <string>
#include <vector>

#include "core/message.h"

struct Config;

// A pluggable ingestion backend: anything that produces Messages
// (Pushover websocket, a future local HTTP listener, MQTT, ...).
//
// Contract:
//  - start() must not block: long-running work happens on the source's own
//    worker thread(s).
//  - Callbacks may be invoked from ANY thread; the app marshals them to the
//    FLTK main thread itself. Sources just call them.
//  - stop() must join/terminate all worker threads before returning and is
//    called before Config is torn down. It must be safe to call twice.
//
// To add a new source: subclass MessageSource, then append an instance in
// create_sources() (src/core/app.cpp). See docs in README.md.
class MessageSource {
public:
  struct Callbacks {
    // Deliver freshly received messages (already acked upstream if the
    // protocol needs acking — display failure must not cause re-delivery).
    std::function<void(std::vector<Message>)> on_messages;
    // Short human-readable connection state ("connected", "reconnecting in 5s").
    std::function<void(std::string)> on_state;
    // Error report; fatal=true means the source stopped itself and needs
    // user intervention (e.g. re-login) — the app surfaces it as a popup.
    std::function<void(std::string, bool fatal)> on_error;
  };

  virtual ~MessageSource() = default;
  virtual const char* name() const = 0;
  // May decide it is not configured (report via on_state) and do nothing.
  virtual void start(const Config& cfg, Callbacks cb) = 0;
  virtual void stop() = 0;
};
