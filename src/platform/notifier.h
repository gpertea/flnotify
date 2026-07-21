#pragma once
#include <memory>
#include "core/message.h"

class Tray;

class Notifier {
public:
  virtual ~Notifier() = default;
  // Display natively. Return false to make the caller use the FLTK popup fallback.
  virtual bool show(const Message& m) = 0;
};

std::unique_ptr<Notifier> create_notifier(Tray& tray);
