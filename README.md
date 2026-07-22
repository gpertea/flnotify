# flnotify

Cross-platform (Windows / Linux / macOS) system-tray app that acts as a
**Pushover Open Client**: it registers as an extra device on your Pushover
account and shows incoming notifications on the desktop in real time —
always-visible popups in the top-right corner (or native OS notifications if
you prefer). Remote scripts keep POSTing to Pushover unchanged; this app just
makes the desktop ring alongside the phone.

Portable app: configuration lives in `flnotify.ini` next to the executable.
No registry, no %APPDATA%, no XDG dirs.

## Build

GNU Make only (no CMake). Requires a C++20 compiler, FLTK 1.4, and libcurl
with websocket support (`curl-config --protocols` must list `WSS`; curl ≥ 8.x).

```sh
make            # builds flnotify(.exe); objects under build/<platform>/
make run
make compile_commands.json   # for clangd
```

On Windows build from an MSYS2 **UCRT64** shell
(`pacman -S mingw-w64-ucrt-x86_64-{gcc,make,fltk,curl}`). Platform specifics
live in `mk/windows.mk`, `mk/linux.mk`, `mk/macos.mk`, selected via `uname -s`.

## First run

On first start (no `flnotify.ini`) the Settings dialog opens: log in with your
Pushover account (2FA supported), pick a device name, done. The websocket
client then connects and messages queued for this device start appearing.
Note: an Open Client counts as a Pushover **desktop** client — 30-day free
trial, then Pushover's one-time desktop license applies to your account. Only
the API `secret` is stored, never your password; treat `flnotify.ini` as
sensitive.

Tray menu: **Settings…**, **Pause notifications**, **Test notification**,
**Quit**.

## Architecture / extending

```
src/core      shared app logic (config, message model, dispatch, logging)
src/ui        shared FLTK ui (settings dialog, popup renderer)
src/net       Pushover client (libcurl REST + websocket)
src/platform  per-platform tray + native notifier behind small interfaces
```

All incoming messages funnel through one dispatch point on the FLTK main
thread, then go to a display backend. Both ends are pluggable:

### Adding a message source (ingestion protocol)

`src/core/source.h` defines the `MessageSource` interface. The Pushover
client is just the first implementation (`src/net/pushover_source.*`). To add
another protocol — e.g. a local HTTP listener, MQTT, a mail poller:

1. Subclass `MessageSource`:
   - `start(cfg, callbacks)` must return immediately; run your protocol on
     your own worker thread.
   - Call `callbacks.on_messages(...)` with `Message` objects as they arrive
     (any thread — the app marshals to the UI thread), `on_state(...)` with
     short connection-state strings, and `on_error(text, fatal)` when the user
     must intervene.
   - `stop()` must join your threads before returning.
2. Register an instance in `create_sources()` in `src/core/app.cpp`.
3. If it needs settings, extend `Config` (`src/core/config.*`) and the
   dialog (`src/ui/config_dialog.cpp`).

Every source's messages get identical display treatment (priority filters,
pause, popups) for free.

### Adding a display backend

`src/platform/notifier.h` defines `Notifier` — implement `show(const Message&)`
returning `false` to fall back to the built-in FLTK popup (`src/ui/popup.cpp`),
and build it in `create_notifier()` for your platform. The popup renderer is
the universal fallback and works everywhere.
