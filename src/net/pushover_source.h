#pragma once
#include "core/source.h"
#include "net/pushover.h"

// MessageSource adapter around the Pushover Open Client (pushover::Client).
class PushoverSource : public MessageSource {
public:
  const char* name() const override { return "Pushover"; }
  void start(const Config& cfg, Callbacks cb) override;
  void stop() override { client_.stop(); }

private:
  pushover::Client client_;
};
