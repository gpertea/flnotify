#pragma once
#include <string>

struct Config {
  // Pushover Open Client credentials (never the account password)
  std::string secret;
  std::string device_id;
  std::string device_name = "flnotify-desktop";

  // Display preferences
  bool paused = false;
  // Use the OS notification system (Windows toasts, subject to Focus Assist /
  // Action Center) instead of the always-visible FLTK popup. Off by default.
  bool native_notifications = false;
  int popup_timeout = 8;                                    // seconds
  bool show_priority[5] = {true, true, true, true, true};   // index = priority + 2
  bool run_at_startup = false;

  bool has_credentials() const { return !secret.empty() && !device_id.empty(); }
};

// Absolute directory of the running executable (not the cwd).
std::string exe_dir();
// exe_dir() + "/flnotify.ini"
std::string config_path();

bool load_config(Config& cfg);   // false if the ini is missing/unreadable
bool save_config(const Config& cfg);
