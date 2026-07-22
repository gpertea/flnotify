#include "ui/config_dialog.h"

#include "core/log.h"
#include "net/pushover.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Return_Button.H>
#include <FL/Fl_Secret_Input.H>
#include <FL/Fl_Spinner.H>
#include <FL/Fl_Window.H>

#include <string>
#include <thread>

namespace {

struct Dialog {
  Config* cfg = nullptr;
  bool changed = false;
  bool open = true;
  bool busy = false;  // a login thread is in flight

  Fl_Window* win = nullptr;
  Fl_Input* email = nullptr;
  Fl_Secret_Input* password = nullptr;
  Fl_Input* twofa = nullptr;
  Fl_Input* devname = nullptr;
  Fl_Button* login_btn = nullptr;
  Fl_Box* status = nullptr;
  Fl_Check_Button* native = nullptr;
  Fl_Spinner* timeout = nullptr;
  Fl_Check_Button* prio[5] = {};

  std::string status_text;  // Fl_Box keeps a pointer; own the storage here
  void set_status(const std::string& s) {
    status_text = s;
    status->label(status_text.c_str());
    win->redraw();
  }
};

Dialog* g_dlg = nullptr;  // only one dialog at a time; null once closed

// Heap job passed to the login thread; results marshalled back via Fl::awake.
struct LoginJob {
  std::string email, password, twofa, devname;
  pushover::Result res;
  std::string secret, device_id;
};

void login_done_cb(void* v) {
  auto* job = static_cast<LoginJob*>(v);
  Dialog* d = g_dlg;
  if (d) {
    d->busy = false;
    d->login_btn->activate();
    if (job->res.ok) {
      d->cfg->secret = job->secret;
      d->cfg->device_id = job->device_id;
      d->cfg->device_name = job->devname;
      save_config(*d->cfg);
      d->changed = true;
      d->password->value("");  // never keep the password around
      d->twofa->value("");
      d->set_status("Logged in — device \"" + job->devname + "\" registered.");
      logx::write("login ok, device registered");
    } else if (job->res.needs_twofa) {
      d->twofa->activate();
      d->twofa->take_focus();
      d->set_status("Enter the two-factor code from your authenticator, then retry.");
    } else {
      d->set_status("Error: " + job->res.error);
      logx::write("login/registration error: " + job->res.error);
    }
  }
  delete job;
}

void login_cb(Fl_Widget*, void* v) {
  auto* d = static_cast<Dialog*>(v);
  if (d->busy) return;
  std::string email = d->email->value();
  std::string password = d->password->value();
  std::string devname = d->devname->value();
  if (email.empty() || password.empty()) {
    d->set_status("Email and password are required.");
    return;
  }
  if (devname.empty() || devname.size() > 25 ||
      devname.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                "abcdefghijklmnopqrstuvwxyz0123456789_-") !=
          std::string::npos) {
    d->set_status("Device name: 1-25 chars, only letters, digits, _ and -.");
    return;
  }
  d->busy = true;
  d->login_btn->deactivate();
  d->set_status("Logging in…");

  auto* job = new LoginJob{email, password, d->twofa->value(), devname, {}, {}, {}};
  std::thread([job] {
    job->res = pushover::login(job->email, job->password, job->twofa, job->secret);
    job->password.clear();
    if (job->res.ok)
      job->res = pushover::register_device(job->secret, job->devname, job->device_id);
    Fl::awake(login_done_cb, job);
  }).detach();
}

void save_cb(Fl_Widget*, void* v) {
  auto* d = static_cast<Dialog*>(v);
  d->cfg->device_name = d->devname->value();
  d->cfg->native_notifications = d->native->value() != 0;
  d->cfg->popup_timeout = (int)d->timeout->value();
  for (int i = 0; i < 5; ++i) d->cfg->show_priority[i] = d->prio[i]->value() != 0;
  save_config(*d->cfg);
  d->changed = true;
  d->open = false;
}

void cancel_cb(Fl_Widget*, void* v) {
  static_cast<Dialog*>(v)->open = false;
}

}  // namespace

bool config_dialog_show(Config& cfg) {
  if (g_dlg) {  // already open: just raise it
    g_dlg->win->show();
    return false;
  }
  Dialog d;
  d.cfg = &cfg;
  g_dlg = &d;

  d.win = new Fl_Window(460, 470, "flnotify — Settings");

  new Fl_Box(10, 8, 440, 20, "Pushover account");
  d.email = new Fl_Input(120, 34, 320, 26, "Email:");
  d.password = new Fl_Secret_Input(120, 66, 320, 26, "Password:");
  d.twofa = new Fl_Input(120, 98, 120, 26, "2FA code:");
  d.twofa->deactivate();  // enabled only when the API asks for it
  d.devname = new Fl_Input(120, 130, 200, 26, "Device name:");
  d.devname->value(cfg.device_name.c_str());
  d.login_btn = new Fl_Button(120, 164, 170, 28, "Log in && register");
  d.login_btn->callback(login_cb, &d);

  d.status = new Fl_Box(15, 200, 430, 40);
  d.status->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
  d.status->labelsize(12);

  new Fl_Box(10, 246, 440, 20, "Display");
  d.native = new Fl_Check_Button(20, 270, 420, 24,
                                 "Use Windows notifications (Focus Assist applies)");
  d.native->value(cfg.native_notifications);
  d.timeout = new Fl_Spinner(120, 300, 70, 24, "Popup (s):");
  d.timeout->range(2, 120);
  d.timeout->value(cfg.popup_timeout);

  new Fl_Box(20, 330, 200, 20, "Show popups for priority:");
  static const char* prio_labels[5] = {"-2", "-1", "0", "1", "2"};
  for (int i = 0; i < 5; ++i) {
    d.prio[i] = new Fl_Check_Button(225 + i * 45, 330, 40, 20, prio_labels[i]);
    d.prio[i]->value(cfg.show_priority[i]);
  }

  auto* note = new Fl_Box(15, 362, 430, 44,
      "Note: an Open Client counts as a Pushover desktop client — free for "
      "30 days, then Pushover's one-time desktop license applies to your account.");
  note->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
  note->labelsize(11);

  auto* save = new Fl_Return_Button(240, 420, 120, 30, "Save && close");
  save->callback(save_cb, &d);
  auto* cancel = new Fl_Button(370, 420, 75, 30, "Cancel");
  cancel->callback(cancel_cb, &d);

  d.win->end();
  d.win->callback(cancel_cb, &d);  // window close button
  if (cfg.has_credentials())
    d.set_status("Logged in — device \"" + cfg.device_name +
                 "\" is registered. Log in again only to re-authenticate.");
  d.win->show();

  while (d.open && d.win->shown())
    Fl::wait();

  // If a login thread is still in flight, wait for it so login_done_cb can't
  // touch a dead dialog (it checks g_dlg, which we clear after the drain).
  while (d.busy)
    Fl::wait();

  g_dlg = nullptr;
  Fl::delete_widget(d.win);
  return d.changed;
}
