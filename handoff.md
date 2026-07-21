# flnotify — Project Handoff

**Project:** `flnotify` — a cross-platform (Windows/Linux/macOS) system-tray application that acts as a **Pushover Open Client**: it registers as a device on the user's Pushover account, receives notifications in real time over a websocket, and displays them as native tray/desktop notifications. Shared C++ codebase, FLTK for the (shared) configuration dialog, thin per-platform modules for tray + notification display.

**Status:** Environment configured and verified. No code written yet. This document is the complete spec for scaffolding and first implementation. Start here.

---

## 1. User intent (the "why")

- The user has a **Pushover account**. Remote scripts (e.g. on a work server, NOT on the LAN) POST to Pushover's API when jobs finish; the user gets phone notifications.
- They want the **same notifications to also appear on this desktop**, top-right corner, as tray notifications — in real time.
- Solution: this app registers as an **additional device** on their Pushover account via the **Open Client API**. Remote scripts change nothing. Desktop dials out (websocket), so NAT/firewall is a non-issue.
- A local HTTP listener for same-machine scripts was considered and **explicitly deferred** — optional phase-2 input source, NOT v1.
- The tray concept maps to: Windows tray, Linux DE status area, macOS menu-bar icon (`NSStatusItem`).

## 2. Environment (already configured — do not redo)

- OS here: Windows 10, user works in **MSYS2 + mintty** (never Windows Terminal/PowerShell as their environment).
- `~/.claude/settings.json` sets `CLAUDE_CODE_GIT_BASH_PATH=C:\Prog\msys2\usr\bin\bash.exe` and `MSYSTEM=UCRT64` → Claude Code sessions use **MSYS2 bash**; use bash syntax and POSIX paths (`/d/_work_/flnotify`, `/c/...`).
- Shell `$HOME` = `C:\Prog\msys2\home\gpertea` (MSYS2), distinct from Claude config dir `C:\Users\gpertea\.claude`. Global build guidance also lives in `~/.claude/CLAUDE.md` (Claude config dir) and project memory.
- **Toolchain (verified working, produces genuinely native x64 exes):** MSYS2 **UCRT64** — g++ 16.1.0 (`/ucrt64/bin/g++`, target `x86_64-w64-mingw32`), cmake 4.3.3 (unused — see §4), GNU Make 4.4.1 (`mingw32-make`; plain `make` exists at `/usr/bin/make` and works for driving builds), ninja, gdb 17.2, pkgconf.
- **NEVER use `/usr/bin/g++`** (msys-2.0.dll dependency = not native).
- **FLTK 1.4.5 installed** (`mingw-w64-ucrt-x86_64-fltk`). Verified: a `-mwindows` FLTK GUI app compiles and links native (deps = KERNEL32 + api-ms-win-crt-* + libfltk/libstdc++ DLLs).
- Verified build recipes:
  - Console/Win32: `g++ -O2 -std=c++20 app.cpp -o app.exe -static-libgcc -static-libstdc++`
  - FLTK GUI: `g++ -O2 -std=c++20 app.cpp -o app.exe $(fltk-config --use-images --cxxflags --ldflags) -mwindows`
- Extra libraries: `pacman -S --needed mingw-w64-ucrt-x86_64-<pkg>`. (User pre-authorized installing needed packages; still say what you install and why.)
- clangd plugin is enabled → generate `compile_commands.json` (see §4).

## 3. Architecture

```
flnotify/
  handoff.md                 (this file)
  Makefile                   common rules; includes mk/<platform>.mk
  mk/
    windows.mk               platform sources, LDFLAGS (-mwindows, -lole32 -lshell32 ...), .exe suffix
    linux.mk                 dbus/appindicator + libnotify flags
    macos.mk                 .mm compilation, -framework Cocoa (+ notification framework)
  src/
    core/                    SHARED: app logic
      app.cpp/.h             wiring, main loop integration (FLTK Fl::run + awake for cross-thread)
      message.h              Message model: id, title, body, priority (-2..2), app, timestamp, url
      config.cpp/.h          load/save config (simple INI or JSON file in per-platform config dir)
      log.cpp/.h             minimal logging to file
    ui/                      SHARED: FLTK
      config_dialog.cpp/.h   account login (email/password/2FA), device name, per-priority prefs
      popup.cpp/.h           FLTK-drawn notification popup — universal fallback renderer
    net/                     SHARED: Pushover client (libcurl)
      pushover.cpp/.h        PushoverClient: login, device registration, websocket loop, fetch, ack
      http.cpp/.h            thin libcurl wrapper (HTTPS GET/POST/DELETE, JSON in/out)
    platform/                per-platform, selected by makefile — implements two small interfaces:
      tray.h                 interface: set icon/tooltip, menu (Open config / Pause / Quit), callbacks
      notifier.h             interface: show(const Message&) as a native notification
      tray_win.cpp           Shell_NotifyIcon + hidden HWND message window
      notify_win.cpp         Shell_NotifyIcon balloon (NIF_INFO) — simplest; toast is optional later
      tray_linux.cpp         StatusNotifierItem via libayatana-appindicator (pragmatic choice)
      notify_linux.cpp       org.freedesktop.Notifications via libnotify (or direct D-Bus)
      tray_mac.mm            NSStatusItem (Obj-C++)
      notify_mac.mm          NSUserNotification (deprecated but works unbundled/unsigned);
                             UNUserNotificationCenter requires signed .app bundle — document, defer
  assets/                    tray icons (ico/png), embedded via .rc on Windows
  third_party/               ONLY if a header-only JSON lib is vendored (see §5)
```

Threading model: websocket/network runs on a worker thread; UI (FLTK) on main thread. Hand messages over with `Fl::awake(callback, data)` — FLTK's documented cross-thread mechanism. Keep the platform Notifier calls on the main thread.

Fallback rule: if a native notifier fails/unavailable (e.g. Linux without a notification daemon, macOS unsigned build), use `ui/popup.cpp` (FLTK top-right popup, auto-dismiss, click-to-open). This also gives identical behavior everywhere for testing.

## 4. Build system — DECIDED, do not relitigate

- **No CMake.** User explicitly dislikes it (unnecessary complication/maintenance). Decision: **one shared GNU Makefile + tiny `mk/<platform>.mk` fragments**, platform picked via `uname -s` (MSYS2 reports `MSYS_NT-*`/`MINGW*`; map UCRT64/MINGW to windows.mk).
- Makefile must: build `.o` into `build/<platform>/`, link `flnotify(.exe)`, have `run`, `clean`, and a `compile_commands.json` target (hand-emit entries per source — trivial in make — or document `bear -- make` on Linux/macOS; clangd is enabled and needs it).
- Windows link flags: `-mwindows` (GUI subsystem), `-static-libgcc -static-libstdc++` preferred; FLTK flags from `$(fltk-config --use-images --cxxflags --ldflags)`.

## 5. Dependencies

| Dep | Purpose | Status / action |
|---|---|---|
| FLTK 1.4.5 | config dialog + fallback popup | installed (UCRT64) |
| libcurl | HTTPS + WebSocket (`wss://`) | **FIRST TASK: verify** MSYS2 `mingw-w64-ucrt-x86_64-curl` is installed and built with websocket support: `curl-config --protocols | grep -i wss` (or check `curl --version` features for `ws`/`wss`). curl ≥ 8.x generally has it. If the UCRT64 curl lacks WSS, options: (a) `curl-winssl` variant, (b) poll fallback (see §6 caveat), (c) tiny ws client over TLS — prefer (a). |
| JSON parsing | Pushover API responses | vendor a single-header lib (nlohmann/json single include, or picojson) into `third_party/` — no build-system impact |
| Linux (later) | tray + notify | `libayatana-appindicator3`, `libnotify` via distro packages |
| macOS (later) | tray + notify | system frameworks only; Obj-C++ `.mm` |

## 6. Pushover Open Client API (the core of v1)

Docs: https://pushover.net/api/client — verify details against live docs; summary of the flow:

1. **Login:** `POST https://api.pushover.net/1/users/login.json` with `email`, `password` (+ `twofa` if user has 2FA) → returns `secret`. Store secret in config; NEVER store the password.
2. **Register device:** `POST https://api.pushover.net/1/devices.json` with `secret`, `name` (e.g. `flnotify-desktop`, user-configurable, [A-Za-z0-9_-], ≤25 chars), `os=O` → returns `id` (device_id). One-time; store device_id.
3. **Download pending messages:** `GET https://api.pushover.net/1/messages.json?secret=...&device_id=...` → array of messages (id, title, message, app, aid, icon, date, priority, acked, umid, sound...). Do this once at startup and after every websocket wake-up.
4. **Acknowledge/delete:** after displaying, `POST https://api.pushover.net/1/devices/<device_id>/update_highest_message.json` with `secret` and `message` = highest message id received. Otherwise messages are re-delivered. Priority-2 (emergency) messages additionally need `POST /1/receipts/<receipt>/acknowledge.json`.
5. **Websocket:** connect `wss://client.pushover.net/push`, then send text frame `login:<device_id>:<secret>\n`. Server sends 1-byte frames: `#` keepalive (~every 30 s), `!` = new message → do step 3+4, `R` = reconnect requested (close & reconnect), `E` = permanent error (re-auth needed — likely device deleted or secret invalidated; prompt re-login), `A` = closed because account logged in elsewhere / session replaced — stop and inform user.
6. **Reconnect** with exponential backoff (e.g. 5 s → cap 5 min) on socket drop or missing keepalives (>60 s without `#`).

Licensing note (tell the user when they first log in, not a blocker): an Open Client counts as a Pushover **desktop** client — 30-day free trial per platform, then Pushover's one-time desktop license (~$5) on their account.

Caveat: if WSS turns out unavailable in the shipped curl, a temporary fallback is polling `messages.json` every N seconds — implement ONLY as a stopgap behind the same PushoverClient interface, and prefer fixing WSS.

## 7. Configuration — portable `flnotify.ini` (USER DECISION) + dialog spec

- **Config is a single local INI file, `flnotify.ini`, in the SAME directory as the executable — identical approach on all three platforms.** Portable app style: no %APPDATA%, no XDG dirs, no registry/plists. Resolve the path from the executable's location (Windows `GetModuleFileName`, Linux `/proc/self/exe`, macOS `_NSGetExecutablePath`), NOT the cwd.
- Contents: Pushover `secret` (from login), `device_id`, device name, display prefs (per-priority popup on/off, popup timeout, pause state; quiet hours/sound later). **Never store the account password.** Human-editable INI with comments; hand-rolled or tiny single-header parser. Treat the file as sensitive (user-only perms on POSIX).
- **Dialog flow:** on **first start** (no ini, or missing/invalid `secret`+`device_id`) the Configuration dialog opens immediately; successful login+device registration writes `flnotify.ini` and starts the websocket client. Afterwards a **"Settings…"** item in the tray/menu-bar icon menu reopens the same dialog.
- Dialog fields: email + password (+ 2FA code when required) → login+device registration, then logged-in state view (account email, device name, connection status). Device name editable before registration.
- Prefs in dialog: run at startup (per-platform impl, Windows: HKCU Run key), per-priority popup toggles, popup timeout.
- Tray menu: Settings…, Pause notifications, Recent messages (later), Quit.

## 8. Suggested implementation order (Windows first, on this machine)

1. Verify curl WSS support (see §5); install/adjust package if needed. Vendor JSON header.
2. Scaffold tree + Makefile + `mk/windows.mk`; commit a `git init` repo (project dir is not yet a git repo).
3. `tray_win.cpp` + stub Notifier → app runs, tray icon, menu with Quit. Test.
4. `notify_win.cpp` balloon + `ui/popup.cpp` fallback → test with a fake injected Message.
5. `net/http.cpp` + login + device registration driven by a minimal `config_dialog.cpp`. Test against the real API with the user's account (interactively — never log credentials).
6. Websocket loop + fetch + ack on worker thread → end-to-end: user sends a real Pushover notification (e.g. from phone/curl) and sees the desktop popup. This is the v1 milestone.
7. Polish dialog prefs, run-at-startup, reconnect/backoff hardening.
8. Linux port (appindicator+libnotify), then macOS port (`.mm`, NSStatusItem + NSUserNotification; document signed-bundle path for UNUserNotificationCenter).

## 9. Testing notes

- Unit-testable pieces: message JSON parsing, config round-trip, backoff logic — plain asserts in a `make test` target is enough; no framework needed.
- End-to-end: `curl -s -F "token=<app_token>" -F "user=<user_key>" -F "message=test" https://api.pushover.net/1/messages.json` from any machine triggers the flow.
- The user wants to *see it work*: after milestone 6, demonstrate with a real send.
