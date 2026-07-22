#pragma once
#include "core/config.h"
#include "core/message.h"

// FLTK-drawn notification popup — universal fallback renderer.
// Placement (screen/corner) and appearance (colors, font size) come from cfg.
// Must be called on the FLTK main thread. Popups stack away from the chosen
// corner, auto-dismiss after timeout_sec; click dismisses (opens m.url if set).
void popup_show(const Message& m, const Config& cfg, int timeout_sec);
