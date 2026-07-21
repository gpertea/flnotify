#include "ui/popup.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Window.H>
#include <FL/filename.H>

#include <string>
#include <vector>

namespace {

constexpr int POPUP_W = 360;
constexpr int POPUP_H = 96;
constexpr int MARGIN = 12;
constexpr int GAP = 8;

class PopupWindow;
std::vector<PopupWindow*> g_active;

void relayout();

class PopupWindow : public Fl_Window {
public:
  PopupWindow(const Message& m, int timeout_sec)
      : Fl_Window(0, 0, POPUP_W, POPUP_H), url_(m.url) {
    border(0);
    set_override();  // no taskbar entry, no focus steal (tooltip-style)
    color(fl_rgb_color(45, 45, 48));

    std::string title = m.title.empty() ? (m.app.empty() ? "flnotify" : m.app) : m.title;
    title_ = strdup(title.c_str());
    body_ = strdup(m.body.c_str());

    auto* t = new Fl_Box(10, 8, POPUP_W - 20, 22, title_);
    t->labelfont(FL_HELVETICA_BOLD);
    t->labelsize(14);
    t->labelcolor(FL_WHITE);
    t->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_TOP);

    auto* b = new Fl_Box(10, 32, POPUP_W - 20, POPUP_H - 40, body_);
    b->labelsize(12);
    b->labelcolor(fl_rgb_color(220, 220, 220));
    b->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);

    end();
    clear_visible_focus();
    Fl::add_timeout(timeout_sec, timeout_cb, this);
  }

  ~PopupWindow() override {
    Fl::remove_timeout(timeout_cb, this);
    free(title_);
    free(body_);
  }

  int handle(int event) override {
    if (event == FL_PUSH) {
      if (!url_.empty()) fl_open_uri(url_.c_str());
      dismiss();
      return 1;
    }
    return Fl_Window::handle(event);
  }

  void dismiss() {
    for (auto it = g_active.begin(); it != g_active.end(); ++it) {
      if (*it == this) { g_active.erase(it); break; }
    }
    Fl::delete_widget(this);  // safe delete from within event handling
    relayout();
  }

private:
  static void timeout_cb(void* v) { static_cast<PopupWindow*>(v)->dismiss(); }

  std::string url_;
  char* title_;
  char* body_;
};

// Stack active popups from the top-right corner downward.
void relayout() {
  int sx, sy, sw, sh;
  Fl::screen_work_area(sx, sy, sw, sh);
  int y = sy + MARGIN;
  for (auto* p : g_active) {
    p->position(sx + sw - POPUP_W - MARGIN, y);
    y += POPUP_H + GAP;
  }
}

}  // namespace

void popup_show(const Message& m, int timeout_sec) {
  auto* p = new PopupWindow(m, timeout_sec);
  g_active.push_back(p);
  relayout();
  p->show();
}
