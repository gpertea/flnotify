#pragma once
#include <functional>
#include <memory>
#include <string>

struct TrayCallbacks {
  std::function<void()> on_settings;
  std::function<void(bool paused)> on_pause_toggle;  // receives the new state
  std::function<void()> on_test;                     // dev helper: inject a fake message
  std::function<void()> on_quit;
};

class Tray {
public:
  virtual ~Tray() = default;
  virtual bool init(const TrayCallbacks& cb) = 0;
  virtual void set_tooltip(const std::string& text) = 0;
  virtual void set_paused(bool paused) = 0;
  // Platform-specific handle the Notifier may need (Windows: HWND owning the tray icon).
  virtual void* native_handle() = 0;
  virtual void shutdown() = 0;
};

std::unique_ptr<Tray> create_tray();
