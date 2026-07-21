#pragma once
#include "core/message.h"

// FLTK-drawn top-right notification popup — universal fallback renderer.
// Must be called on the FLTK main thread. Popups stack downward and
// auto-dismiss after timeout_sec; click dismisses (and opens m.url if set).
void popup_show(const Message& m, int timeout_sec);
