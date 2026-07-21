#pragma once
#include <string>

namespace logx {
// Open (append) the log file; safe to call once at startup.
void init(const std::string& path);
// Timestamped line; thread-safe. Never pass credentials/secrets.
void write(const std::string& line);
}
