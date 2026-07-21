#include <FL/Fl.H>

#include "core/config.h"
#include "core/log.h"
#include "core/message.h"
#include "platform/notifier.h"
#include "platform/tray.h"
#include "ui/popup.h"

#include <ctime>
#include <memory>

namespace {

bool g_quit = false;
Config g_cfg;
std::unique_ptr<Tray> g_tray;
std::unique_ptr<Notifier> g_notifier;

// Single dispatch point for every incoming message (real or test).
// Must run on the FLTK main thread.
void show_message(const Message& m) {
  if (g_cfg.paused) return;
  if (m.priority >= -2 && m.priority <= 2 && !g_cfg.show_priority[m.priority + 2]) return;
  if (!g_notifier || !g_notifier->show(m))
    popup_show(m, g_cfg.popup_timeout);
}

}  // namespace

int main() {
  Fl::lock();  // enable FLTK multithread support (worker thread will use Fl::awake)

  logx::init(exe_dir() + "/flnotify.log");
  load_config(g_cfg);
  logx::write(g_cfg.has_credentials() ? "startup: config loaded"
                                      : "startup: no credentials yet");

  g_tray = create_tray();
  TrayCallbacks cb;
  cb.on_settings = [] {
    // TODO step 5: open the FLTK configuration dialog
    Message m;
    m.title = "Settings";
    m.body = "The configuration dialog is not implemented yet.";
    popup_show(m, 5);
  };
  cb.on_pause_toggle = [](bool paused) {
    g_cfg.paused = paused;
    save_config(g_cfg);
    g_tray->set_tooltip(paused ? "flnotify (paused)" : "flnotify");
  };
  cb.on_test = [] {
    Message m;
    m.title = "Test notification";
    m.app = "flnotify";
    m.body = "If you can read this, the notification path works.";
    m.priority = 0;
    m.date = std::time(nullptr);
    show_message(m);
  };
  cb.on_quit = [] { g_quit = true; };

  if (!g_tray->init(cb)) {
    logx::write("fatal: tray init failed");
    return 1;
  }
  g_tray->set_paused(g_cfg.paused);
  g_tray->set_tooltip(g_cfg.paused ? "flnotify (paused)" : "flnotify");
  g_notifier = create_notifier(*g_tray);

  // TODO step 5/6: config dialog on first start, then websocket worker thread.

  while (!g_quit)
    Fl::wait(0.5);

  g_tray->shutdown();
  logx::write("shutdown");
  return 0;
}
