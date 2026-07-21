# flnotify

Cross-platform (Windows / Linux / macOS) system-tray app that acts as a **Pushover Open Client**: it registers as an extra device on the user's Pushover account and displays incoming Pushover notifications on the desktop in real time (tray notifications on Windows/Linux; menu-bar icon on macOS). Remote scripts keep POSTing to Pushover unchanged — this app just makes the desktop ring alongside the phone.

**Read `handoff.md` first** — it is the full spec: architecture, Pushover Open Client protocol flow, dependency status, implementation order. This file records the standing project rules.

## Fixed decisions (do not relitigate)

- **Build system: GNU Makefile only — NO CMake.** One shared `Makefile` + small `mk/<platform>.mk` fragments selected via `uname -s`. Keep a `compile_commands.json` target (clangd is enabled).
- **UI toolkit: FLTK 1.4** (installed in MSYS2 UCRT64). The configuration dialog is shared code, identical on all platforms.
- **Shared codebase**: `src/core`, `src/ui`, `src/net` are platform-independent; only `src/platform/` (tray + notifier implementations) is per-platform, behind the small `tray.h` / `notifier.h` interfaces.
- **Windows builds**: MSYS2 **UCRT64** g++ only (`/ucrt64/bin/g++`), never `/usr/bin/g++`. GUI apps link with `-mwindows`; prefer `-static-libgcc -static-libstdc++`. FLTK flags via `$(fltk-config --use-images --cxxflags --ldflags)`.
- **Ingestion is Pushover only for v1** — a local HTTP listener was considered and deferred.

## Configuration — portable `flnotify.ini`

- **Single local INI file, `flnotify.ini`, located in the same directory as the executable — same approach on all three platforms.** The app is portable; no %APPDATA%, no XDG dirs, no registry, no plists for config.
- Resolve the ini path from the *executable's* directory (Windows `GetModuleFileName`, Linux `/proc/self/exe`, macOS `_NSGetExecutablePath`) — **not** from the current working directory.
- Contents: Pushover credentials/tokens (`secret` from login, `device_id`, device name) plus display options (per-priority popup on/off, popup timeout, pause state, etc.). Never store the account password — only the API `secret`.
- Plain INI format, hand-rolled or tiny single-header parser; keep it human-editable with comments.

## Configuration dialog flow

- **First start** (no `flnotify.ini`, or ini lacks valid `secret`/`device_id`): immediately open the **Configuration dialog** (Pushover login → device registration), before/while the tray icon appears. On successful login, write `flnotify.ini` and start the websocket client.
- **After that**: the tray/menu-bar icon menu has a **"Settings…"** item that reopens the same dialog for changes (re-login, device name, display prefs). Menu also has at least: Pause notifications, Quit.

## Conventions

- C++20, `-O2`, warnings on (`-Wall -Wextra`). Objective-C++ (`.mm`) only under `src/platform/` for macOS.
- Network/websocket work runs on a worker thread; all UI and Notifier calls on the FLTK main thread via `Fl::awake()`.
- Objects out of `build/<platform>/`; binary is `flnotify` / `flnotify.exe` at repo root or `build/`.
- Never log or echo credentials/secrets; treat `flnotify.ini` as sensitive.
