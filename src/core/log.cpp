#include "core/log.h"

#include <cstdio>
#include <ctime>
#include <mutex>

namespace {
std::FILE* g_file = nullptr;
std::mutex g_mutex;
}

namespace logx {

void init(const std::string& path) {
  std::lock_guard<std::mutex> lk(g_mutex);
  if (g_file) std::fclose(g_file);
  g_file = std::fopen(path.c_str(), "a");
}

void write(const std::string& line) {
  std::lock_guard<std::mutex> lk(g_mutex);
  if (!g_file) return;
  std::time_t now = std::time(nullptr);
  char stamp[32];
  std::strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", std::localtime(&now));
  std::fprintf(g_file, "%s %s\n", stamp, line.c_str());
  std::fflush(g_file);
}

}
