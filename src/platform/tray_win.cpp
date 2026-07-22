#ifdef _WIN32
#include "platform/tray.h"
#include "platform/win_util.h"

#include "core/log.h"

#include <shellapi.h>

namespace {

constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT TRAY_UID = 1;

enum MenuId : UINT {
  ID_SETTINGS = 100,
  ID_PAUSE,
  ID_TEST,
  ID_QUIT,
};

class WinTray : public Tray {
public:
  bool init(const TrayCallbacks& cb) override {
    cb_ = cb;
    taskbar_created_msg_ = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSW wc{};
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"flnotify_tray";
    RegisterClassW(&wc);

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"flnotify", 0, 0, 0, 0, 0,
                            HWND_MESSAGE, nullptr, wc.hInstance, this);
    if (!hwnd_) {
      logx::write("tray: CreateWindowExW failed, error " +
                  std::to_string(GetLastError()));
      return false;
    }
    return add_icon();
  }

  void set_tooltip(const std::string& text) override {
    tooltip_ = text;
    NOTIFYICONDATAW nid = base_nid();
    nid.uFlags = NIF_TIP;
    lstrcpynW(nid.szTip, widen(text).c_str(), ARRAYSIZE(nid.szTip));
    Shell_NotifyIconW(NIM_MODIFY, &nid);
  }

  void set_paused(bool paused) override { paused_ = paused; }

  void* native_handle() override { return hwnd_; }

  void shutdown() override {
    if (hwnd_) {
      NOTIFYICONDATAW nid = base_nid();
      Shell_NotifyIconW(NIM_DELETE, &nid);
      DestroyWindow(hwnd_);
      hwnd_ = nullptr;
    }
  }

private:
  NOTIFYICONDATAW base_nid() {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof nid;
    nid.hWnd = hwnd_;
    nid.uID = TRAY_UID;
    return nid;
  }

  bool add_icon() {
    NOTIFYICONDATAW nid = base_nid();
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));  // IDI_APPLICATION; TODO: real icon via .rc
    lstrcpynW(nid.szTip, widen(tooltip_.empty() ? "flnotify" : tooltip_).c_str(),
              ARRAYSIZE(nid.szTip));
    // NIM_ADD can fail for a while (Explorer purging the icon of a just-killed
    // previous instance, or the taskbar not yet up during Windows login) —
    // retry patiently before giving up.
    DWORD last_err = (DWORD)-1;
    for (int attempt = 0; attempt < 30; ++attempt) {
      if (Shell_NotifyIconW(NIM_ADD, &nid)) return true;
      DWORD err = GetLastError();
      if (Shell_NotifyIconW(NIM_MODIFY, &nid)) return true;  // already registered
      if (err != last_err) {
        logx::write("tray: NIM_ADD failed, error " + std::to_string(err));
        last_err = err;
      }
      Sleep(1000);
    }
    return false;
  }

  void show_menu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_SETTINGS, L"Settings…");
    AppendMenuW(menu, MF_STRING | (paused_ ? MF_CHECKED : 0), ID_PAUSE,
                L"Pause notifications");
    AppendMenuW(menu, MF_STRING, ID_TEST, L"Test notification");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_QUIT, L"Quit");

    POINT pt;
    GetCursorPos(&pt);
    // Required so the menu closes when clicking elsewhere (MS documented quirk).
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
    PostMessageW(hwnd_, WM_NULL, 0, 0);
    DestroyMenu(menu);
  }

  void on_command(UINT id) {
    switch (id) {
      case ID_SETTINGS:
        if (cb_.on_settings) cb_.on_settings();
        break;
      case ID_PAUSE:
        paused_ = !paused_;
        if (cb_.on_pause_toggle) cb_.on_pause_toggle(paused_);
        break;
      case ID_TEST:
        if (cb_.on_test) cb_.on_test();
        break;
      case ID_QUIT:
        if (cb_.on_quit) cb_.on_quit();
        break;
    }
  }

  static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    WinTray* self;
    if (msg == WM_CREATE) {
      self = static_cast<WinTray*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
      self->hwnd_ = hwnd;
      return 0;
    }
    self = reinterpret_cast<WinTray*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

    if (msg == WM_TRAY) {
      switch (LOWORD(lp)) {
        case WM_RBUTTONUP:
        case WM_LBUTTONUP:
        case WM_CONTEXTMENU:
          self->show_menu();
          break;
      }
      return 0;
    }
    if (msg == WM_COMMAND) {
      self->on_command(LOWORD(wp));
      return 0;
    }
    if (msg == self->taskbar_created_msg_) {
      // Explorer restarted: the tray area is fresh, re-add our icon.
      self->add_icon();
      return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
  }

  TrayCallbacks cb_;
  HWND hwnd_ = nullptr;
  UINT taskbar_created_msg_ = 0;
  bool paused_ = false;
  std::string tooltip_;
};

}  // namespace

std::unique_ptr<Tray> create_tray() {
  return std::make_unique<WinTray>();
}
#endif
