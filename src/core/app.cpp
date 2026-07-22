#include <FL/Fl.H>

#include "core/config.h"
#include "core/log.h"
#include "core/message.h"
#include "core/source.h"
#include "net/http.h"
#include "net/pushover_source.h"
#include "platform/notifier.h"
#include "platform/tray.h"
#include "ui/config_dialog.h"
#include "ui/popup.h"

#include <ctime>
#include <memory>
#include <string>
#include <vector>

namespace {

bool g_quit = false;
Config g_cfg;
std::unique_ptr<Tray> g_tray;
std::unique_ptr<Notifier> g_notifier;
std::vector<std::unique_ptr<MessageSource>> g_sources;

// Registration point for ingestion backends — add new MessageSource
// subclasses here (see core/source.h and README.md "Extending").
std::vector<std::unique_ptr<MessageSource>> create_sources() {
  std::vector<std::unique_ptr<MessageSource>> v;
  v.push_back(std::make_unique<PushoverSource>());
  return v;
}

// Single dispatch point for every incoming message, whatever the source.
// Must run on the FLTK main thread.
void show_message(const Message& m) {
  if (g_cfg.paused) return;
  if (m.priority >= -2 && m.priority <= 2 && !g_cfg.show_priority[m.priority + 2]) return;
  if (!g_cfg.native_notifications || !g_notifier || !g_notifier->show(m))
    popup_show(m, g_cfg.popup_timeout);
}

void update_tooltip(const std::string& conn_state) {
  std::string tip = "flnotify";
  if (g_cfg.paused) tip += " (paused)";
  if (!conn_state.empty()) tip += " — " + conn_state;
  g_tray->set_tooltip(tip);
}

// ---- Fl::awake handlers: source worker threads -> main thread ----

void awake_messages(void* v) {
  auto* msgs = static_cast<std::vector<Message>*>(v);
  for (const auto& m : *msgs) show_message(m);
  delete msgs;
}

void awake_state(void* v) {
  auto* s = static_cast<std::string*>(v);
  update_tooltip(*s);
  delete s;
}

struct SourceError {
  std::string source;
  std::string text;
  bool fatal;
};

void awake_error(void* v) {
  auto* e = static_cast<SourceError*>(v);
  Message m;
  m.title = "flnotify — " + e->source + (e->fatal ? " stopped" : " error");
  m.body = e->text;
  popup_show(m, 15);
  if (e->fatal) update_tooltip(e->source + ": needs attention");
  delete e;
}

void start_sources() {
  for (auto& src : g_sources) {
    std::string sname = src->name();
    MessageSource::Callbacks cb;
    cb.on_messages = [](std::vector<Message> msgs) {
      Fl::awake(awake_messages, new std::vector<Message>(std::move(msgs)));
    };
    cb.on_state = [](std::string s) {
      Fl::awake(awake_state, new std::string(std::move(s)));
    };
    cb.on_error = [sname](std::string text, bool fatal) {
      Fl::awake(awake_error, new SourceError{sname, std::move(text), fatal});
    };
    src->start(g_cfg, std::move(cb));
  }
}

void stop_sources() {
  for (auto& src : g_sources) src->stop();
}

void open_settings() {
  if (config_dialog_show(g_cfg)) {
    update_tooltip("");
    // Config changed: restart sources so they pick up new credentials/prefs.
    stop_sources();
    start_sources();
  }
}

}  // namespace

int main() {
  Fl::lock();  // enable FLTK multithread support (workers use Fl::awake)

  logx::init(exe_dir() + "/flnotify.log");
  if (!http::global_init()) {
    logx::write("fatal: curl init failed");
    return 1;
  }
  load_config(g_cfg);
  logx::write(g_cfg.has_credentials() ? "startup: config loaded"
                                      : "startup: no credentials yet");

  g_tray = create_tray();
  TrayCallbacks cb;
  cb.on_settings = [] { open_settings(); };
  cb.on_pause_toggle = [](bool paused) {
    g_cfg.paused = paused;
    save_config(g_cfg);
    update_tooltip("");
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
  update_tooltip("");
  g_notifier = create_notifier(*g_tray);

  g_sources = create_sources();
  if (g_cfg.has_credentials())
    start_sources();
  else
    open_settings();  // first start: configure, which then starts the sources

  while (!g_quit)
    Fl::wait(0.5);

  stop_sources();
  g_tray->shutdown();
  http::global_cleanup();
  logx::write("shutdown");
  return 0;
}
