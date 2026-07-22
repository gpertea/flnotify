#include "ui/popup.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Window.H>
#include <FL/filename.H>
#include <FL/platform.H>

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>
#include <vector>

namespace {

constexpr int POPUP_W = 360;
constexpr int MARGIN = 12;
constexpr int GAP = 8;

// Style snapshot, refreshed from Config on every popup_show(); relayout()
// uses it so all visible popups follow the most recent settings.
struct Style {
  int screen = 0;
  int corner = 0;  // 0 top-right, 1 top-left, 2 bottom-right, 3 bottom-left
  int font = 12;
  Fl_Color bg = fl_rgb_color(0x2d, 0x2d, 0x30);
  Fl_Color fg = fl_rgb_color(0xdc, 0xdc, 0xdc);
} g_style;

Fl_Color hex_color(const std::string& s, unsigned dflt) {
  unsigned v = parse_hex_rgb(s, dflt);
  return fl_rgb_color((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
}

int popup_height() { return 56 + g_style.font * 3; }

class PopupWindow;
std::vector<PopupWindow*> g_active;

void relayout();

class PopupWindow : public Fl_Window {
public:
  PopupWindow(const Message& m, int timeout_sec)
      : Fl_Window(0, 0, POPUP_W, popup_height()), url_(m.url) {
    border(0);
    set_override();  // no taskbar entry, no focus steal (tooltip-style)
    color(g_style.bg);

    std::string title = m.title.empty() ? (m.app.empty() ? "flnotify" : m.app) : m.title;
    title_ = strdup(title.c_str());
    body_ = strdup(m.body.c_str());

    int fs = g_style.font;
    auto* t = new Fl_Box(10, 8, POPUP_W - 20, fs + 10, title_);
    t->labelfont(FL_HELVETICA_BOLD);
    t->labelsize(fs + 2);
    t->labelcolor(g_style.fg);
    t->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_TOP);

    auto* b = new Fl_Box(10, fs + 22, POPUP_W - 20, h() - fs - 30, body_);
    b->labelsize(fs);
    b->labelcolor(g_style.fg);
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

// Stack active popups from the configured corner of the configured screen,
// growing away from that corner.
void relayout() {
  int sx, sy, sw, sh;
  int screen = g_style.screen < Fl::screen_count() ? g_style.screen : 0;
  Fl::screen_work_area(sx, sy, sw, sh, screen);
  const bool at_right = g_style.corner == 0 || g_style.corner == 2;
  const bool at_top = g_style.corner <= 1;
  int y = at_top ? sy + MARGIN : sy + sh - MARGIN;
  for (auto* p : g_active) {
    int x = at_right ? sx + sw - p->w() - MARGIN : sx + MARGIN;
    if (at_top) {
      p->position(x, y);
      y += p->h() + GAP;
    } else {
      y -= p->h();
      p->position(x, y);
      y -= GAP;
    }
  }
}

}  // namespace

void popup_show(const Message& m, const Config& cfg, int timeout_sec) {
  g_style.screen = cfg.popup_screen;
  g_style.corner = cfg.popup_corner;
  g_style.font = cfg.popup_font_size;
  g_style.bg = hex_color(cfg.popup_bg, 0x2d2d30);
  g_style.fg = hex_color(cfg.popup_fg, 0xdcdcdc);

  auto* p = new PopupWindow(m, timeout_sec);
  g_active.push_back(p);
  relayout();
  p->show();
  // Positioning before show() is not honored for borderless/override windows
  // on Windows (they come up at the cursor) — enforce it again on the live
  // window, and pass explicit coordinates to SetWindowPos as well.
  relayout();
#ifdef _WIN32
  // Topmost only — position is FLTK's job: p->x()/y() are logical (DPI-scaled)
  // units and must NOT be fed to SetWindowPos, which takes physical pixels.
  SetWindowPos(fl_xid(p), HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#endif
}
