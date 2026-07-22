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
  // Popup placement: FLTK screen index (0 = primary) and corner
  // (0 = top-right, 1 = top-left, 2 = bottom-right, 3 = bottom-left).
  int popup_screen = 0;
  int popup_corner = 0;
  // Popup appearance: RRGGBB hex colors and label font size.
  std::string popup_bg = "2d2d30";
  std::string popup_fg = "dcdcdc";
  int popup_font_size = 12;
  int popup_timeout = 8;                                    // seconds
  bool show_priority[5] = {true, true, true, true, true};   // index = priority + 2
  bool run_at_startup = false;

  bool has_credentials() const { return !secret.empty() && !device_id.empty(); }
};

// "rrggbb" / "#rrggbb" -> 0xRRGGBB; returns dflt on malformed input.
unsigned parse_hex_rgb(const std::string& s, unsigned dflt);
std::string rgb_to_hex(unsigned rgb);

// Absolute directory of the running executable (not the cwd).
std::string exe_dir();
// exe_dir() + "/flnotify.ini"
std::string config_path();

bool load_config(Config& cfg);   // false if the ini is missing/unreadable
bool save_config(const Config& cfg);
