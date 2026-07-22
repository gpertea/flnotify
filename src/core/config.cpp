#include "core/config.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <climits>
#else
#include <unistd.h>
#include <climits>
#endif

unsigned parse_hex_rgb(const std::string& s, unsigned dflt) {
  std::string t = s;
  if (!t.empty() && t[0] == '#') t.erase(0, 1);
  if (t.size() != 6) return dflt;
  char* end = nullptr;
  unsigned v = (unsigned)std::strtoul(t.c_str(), &end, 16);
  return (end && *end == '\0') ? v : dflt;
}

std::string rgb_to_hex(unsigned rgb) {
  char buf[8];
  std::snprintf(buf, sizeof buf, "%06x", rgb & 0xffffff);
  return buf;
}

std::string exe_dir() {
#ifdef _WIN32
  wchar_t wpath[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, wpath, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return ".";
  int len = WideCharToMultiByte(CP_UTF8, 0, wpath, (int)n, nullptr, 0, nullptr, nullptr);
  std::string path(len, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wpath, (int)n, path.data(), len, nullptr, nullptr);
  for (auto& c : path) if (c == '\\') c = '/';
#elif defined(__APPLE__)
  char buf[PATH_MAX];
  uint32_t size = sizeof buf;
  if (_NSGetExecutablePath(buf, &size) != 0) return ".";
  std::string path(buf);
#else
  char buf[PATH_MAX];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
  if (n <= 0) return ".";
  buf[n] = '\0';
  std::string path(buf);
#endif
  auto slash = path.find_last_of('/');
  return slash == std::string::npos ? "." : path.substr(0, slash);
}

std::string config_path() {
  return exe_dir() + "/flnotify.ini";
}

namespace {

std::string trim(const std::string& s) {
  auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  auto e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

bool to_bool(const std::string& v, bool dflt) {
  if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
  if (v == "0" || v == "false" || v == "no" || v == "off") return false;
  return dflt;
}

}

bool load_config(Config& cfg) {
  std::ifstream in(config_path());
  if (!in) return false;
  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = trim(line.substr(0, eq));
    std::string val = trim(line.substr(eq + 1));
    if (key == "secret") cfg.secret = val;
    else if (key == "device_id") cfg.device_id = val;
    else if (key == "device_name") cfg.device_name = val;
    else if (key == "paused") cfg.paused = to_bool(val, cfg.paused);
    else if (key == "native_notifications")
      cfg.native_notifications = to_bool(val, cfg.native_notifications);
    else if (key == "popup_timeout") cfg.popup_timeout = std::max(1, std::atoi(val.c_str()));
    else if (key == "popup_screen")
      cfg.popup_screen = std::clamp(std::atoi(val.c_str()), 0, 15);
    else if (key == "popup_corner")
      cfg.popup_corner = std::clamp(std::atoi(val.c_str()), 0, 3);
    else if (key == "popup_bg") cfg.popup_bg = val;
    else if (key == "popup_fg") cfg.popup_fg = val;
    else if (key == "popup_font_size")
      cfg.popup_font_size = std::clamp(std::atoi(val.c_str()), 8, 32);
    else if (key == "run_at_startup") cfg.run_at_startup = to_bool(val, cfg.run_at_startup);
    else if (key.rfind("show_priority_", 0) == 0) {
      int p = std::atoi(key.c_str() + std::strlen("show_priority_"));
      if (p >= -2 && p <= 2) cfg.show_priority[p + 2] = to_bool(val, true);
    }
  }
  return true;
}

bool save_config(const Config& cfg) {
  std::ofstream out(config_path(), std::ios::trunc);
  if (!out) return false;
  out << "; flnotify configuration — this file sits next to the executable (portable app).\n"
         "; The secret below grants access to your Pushover messages: keep this file private.\n"
         "\n[pushover]\n"
         "secret = " << cfg.secret << "\n"
         "device_id = " << cfg.device_id << "\n"
         "device_name = " << cfg.device_name << "\n"
         "\n[display]\n"
         "paused = " << (cfg.paused ? 1 : 0) << "\n"
         "; 1 = use OS notifications (Windows toasts — hidden by Focus Assist unless\n"
         ";     this app is allowed); 0 = always-visible built-in popup (default)\n"
         "native_notifications = " << (cfg.native_notifications ? 1 : 0) << "\n"
         "popup_timeout = " << cfg.popup_timeout << "\n"
         "run_at_startup = " << (cfg.run_at_startup ? 1 : 0) << "\n"
         "; popup placement: screen 0 = primary display; corner 0 = top-right,\n"
         "; 1 = top-left, 2 = bottom-right, 3 = bottom-left\n"
         "popup_screen = " << cfg.popup_screen << "\n"
         "popup_corner = " << cfg.popup_corner << "\n"
         "; popup colors (RRGGBB hex) and font size\n"
         "popup_bg = " << cfg.popup_bg << "\n"
         "popup_fg = " << cfg.popup_fg << "\n"
         "popup_font_size = " << cfg.popup_font_size << "\n"
         "; per-priority popup toggles (-2 lowest .. 2 emergency)\n";
  for (int p = -2; p <= 2; ++p)
    out << "show_priority_" << p << " = " << (cfg.show_priority[p + 2] ? 1 : 0) << "\n";
  return out.good();
}
