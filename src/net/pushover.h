#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "core/message.h"

namespace pushover {

struct Result {
  bool ok = false;
  bool needs_twofa = false;   // login answered HTTP 412: retry with a 2FA code
  std::string error;          // human-readable error when !ok
};

// POST /1/users/login.json — never stores/logs the password.
Result login(const std::string& email, const std::string& password,
             const std::string& twofa, std::string& secret_out);

// POST /1/devices.json (os=O) — one-time device registration.
Result register_device(const std::string& secret, const std::string& name,
                       std::string& device_id_out);

// GET /1/messages.json — all messages queued for this device.
Result fetch_messages(const std::string& secret, const std::string& device_id,
                      std::vector<Message>& out);

// POST /1/devices/<id>/update_highest_message.json — ack so nothing re-delivers.
Result ack_highest(const std::string& secret, const std::string& device_id,
                   int64_t highest_id);

// Websocket client: owns a worker thread that connects to
// wss://client.pushover.net/push, authenticates, and on every wake-up frame
// fetches + acks pending messages. ALL callbacks run on the worker thread —
// the receiver must marshal to the UI thread (Fl::awake).
class Client {
public:
  struct Callbacks {
    std::function<void(std::vector<Message>)> on_messages;
    std::function<void(std::string)> on_state;  // "connected", "reconnecting in Ns", ...
    std::function<void()> on_auth_failed;       // server frame 'E': re-login required
    std::function<void()> on_replaced;          // server frame 'A': session replaced
  };

  ~Client() { stop(); }
  void start(std::string secret, std::string device_id, Callbacks cb);
  void stop();
  bool running() const { return thread_.joinable(); }

private:
  void run();
  void fetch_and_deliver();

  std::string secret_, device_id_;
  Callbacks cb_;
  std::atomic<bool> stop_{false};
  std::thread thread_;
};

}  // namespace pushover
