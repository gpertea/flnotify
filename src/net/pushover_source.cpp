#include "net/pushover_source.h"

#include "core/config.h"

void PushoverSource::start(const Config& cfg, Callbacks cb) {
  if (!cfg.has_credentials()) {
    if (cb.on_state) cb.on_state("not configured");
    return;
  }
  pushover::Client::Callbacks pcb;
  pcb.on_messages = cb.on_messages;
  pcb.on_state = cb.on_state;
  pcb.on_auth_failed = [cb] {
    if (cb.on_error)
      cb.on_error(
          "Pushover rejected this device's credentials (removed or revoked). "
          "Open Settings from the tray menu to log in again.",
          true);
  };
  pcb.on_replaced = [cb] {
    if (cb.on_error)
      cb.on_error(
          "This device's session was replaced by another login. "
          "Notifications are stopped; re-login via Settings to resume.",
          true);
  };
  client_.start(cfg.secret, cfg.device_id, std::move(pcb));
}
