#pragma once
#include "core/config.h"

// Modal-ish FLTK configuration dialog (Pushover login + display prefs).
// Blocks in its own Fl::wait loop until closed. Returns true if cfg was
// changed (and already saved to flnotify.ini) — the caller should then
// (re)start the websocket client if credentials are present.
bool config_dialog_show(Config& cfg);
