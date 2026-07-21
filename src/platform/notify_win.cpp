#ifdef _WIN32
#include "platform/notifier.h"
#include "platform/tray.h"
#include "platform/win_util.h"

#include <shellapi.h>

namespace {

// Balloon notification on the existing tray icon (NIF_INFO). Windows 10 renders
// these as regular toast-style popups in the bottom-right corner.
class WinNotifier : public Notifier {
public:
  explicit WinNotifier(HWND tray_hwnd) : hwnd_(tray_hwnd) {}

  bool show(const Message& m) override {
    if (!hwnd_) return false;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof nid;
    nid.hWnd = hwnd_;
    nid.uID = 1;  // TRAY_UID from tray_win.cpp
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    std::string title = m.title.empty() ? (m.app.empty() ? "flnotify" : m.app) : m.title;
    lstrcpynW(nid.szInfoTitle, widen(title).c_str(), ARRAYSIZE(nid.szInfoTitle));
    lstrcpynW(nid.szInfo, widen(m.body).c_str(), ARRAYSIZE(nid.szInfo));
    return Shell_NotifyIconW(NIM_MODIFY, &nid);
  }

private:
  HWND hwnd_;
};

}  // namespace

std::unique_ptr<Notifier> create_notifier(Tray& tray) {
  return std::make_unique<WinNotifier>(static_cast<HWND>(tray.native_handle()));
}
#endif
