#pragma once
#include <cstdint>
#include <ctime>
#include <string>

// One Pushover message as delivered by GET /1/messages.json
struct Message {
  int64_t id = 0;
  std::string umid;       // unique message id (for receipts/dedup)
  std::string title;      // may be empty; fall back to app name for display
  std::string body;       // "message" field
  std::string app;        // sending application name
  std::string url;
  std::string url_title;
  std::string sound;
  std::string icon;
  std::string receipt;    // set only for priority 2 (emergency)
  int priority = 0;       // -2 .. 2
  std::time_t date = 0;
  bool acked = false;
};
