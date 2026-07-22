#include "net/pushover.h"

#include "core/log.h"
#include "net/http.h"

#include <curl/curl.h>
#include <json.hpp>

#include <chrono>

using nlohmann::json;

namespace pushover {

namespace {

const char* API = "https://api.pushover.net/1";

// Pushover reports errors either as an array of strings or an object of
// field -> [messages]; flatten whichever shape arrives.
std::string api_errors(const json& j) {
  std::string out;
  auto add = [&out](const std::string& s) {
    if (!out.empty()) out += "; ";
    out += s;
  };
  if (!j.contains("errors")) return out;
  const auto& e = j["errors"];
  if (e.is_array()) {
    for (const auto& s : e)
      if (s.is_string()) add(s.get<std::string>());
  } else if (e.is_object()) {
    for (const auto& [field, msgs] : e.items()) {
      if (msgs.is_array())
        for (const auto& s : msgs)
          if (s.is_string()) add(field + " " + s.get<std::string>());
    }
  }
  return out;
}

// Log an unexpected response body for debugging — but never a secret.
void log_body(const char* what, const HttpResponse& resp) {
  std::string note;
  if (resp.body.find("secret") != std::string::npos)
    note = "(body contains a secret — redacted)";
  else
    note = resp.body.substr(0, 300);
  logx::write(std::string(what) + ": HTTP " + std::to_string(resp.status) +
              ", body: " + note);
}

Result api_fail(const HttpResponse& resp, const char* what) {
  Result r;
  if (resp.status == 0) {
    r.error = std::string(what) + ": network error: " + resp.error;
    return r;
  }
  std::string detail;
  try {
    detail = api_errors(json::parse(resp.body));
  } catch (...) {}
  r.error = std::string(what) + " failed (HTTP " + std::to_string(resp.status) + ")";
  if (!detail.empty()) r.error += ": " + detail;
  return r;
}

}  // namespace

Result login(const std::string& email, const std::string& password,
             const std::string& twofa, std::string& secret_out) {
  std::map<std::string, std::string> fields{{"email", email}, {"password", password}};
  if (!twofa.empty()) fields["twofa"] = twofa;
  auto resp = http::post_form(std::string(API) + "/users/login.json", fields);
  if (resp.status == 412) {
    Result r;
    r.needs_twofa = true;
    r.error = "two-factor code required";
    return r;
  }
  if (!resp.ok()) return api_fail(resp, "login");
  try {
    auto j = json::parse(resp.body);
    if (j.contains("secret") && j["secret"].is_string()) {
      secret_out = j["secret"].get<std::string>();
      Result r;
      r.ok = true;
      return r;
    }
  } catch (const std::exception& e) {
    logx::write(std::string("login: JSON parse error: ") + e.what());
  }
  log_body("login: unexpected response", resp);
  Result r;
  r.error = "login: unexpected response (HTTP " + std::to_string(resp.status) +
            ") — see flnotify.log";
  return r;
}

Result register_device(const std::string& secret, const std::string& name,
                       std::string& device_id_out) {
  auto resp = http::post_form(std::string(API) + "/devices.json",
                              {{"secret", secret}, {"name", name}, {"os", "O"}});
  if (!resp.ok()) return api_fail(resp, "device registration");
  try {
    auto j = json::parse(resp.body);
    if (j.contains("id") && j["id"].is_string()) {
      device_id_out = j["id"].get<std::string>();
      Result r;
      r.ok = true;
      return r;
    }
  } catch (const std::exception& e) {
    logx::write(std::string("device registration: JSON parse error: ") + e.what());
  }
  log_body("device registration: unexpected response", resp);
  Result r;
  r.error = "device registration: unexpected response (HTTP " +
            std::to_string(resp.status) + ") — see flnotify.log";
  return r;
}

Result fetch_messages(const std::string& secret, const std::string& device_id,
                      std::vector<Message>& out) {
  auto resp = http::get(std::string(API) + "/messages.json?secret=" + secret +
                        "&device_id=" + device_id);
  if (!resp.ok()) return api_fail(resp, "message fetch");
  try {
    auto j = json::parse(resp.body);
    for (const auto& jm : j.value("messages", json::array())) {
      Message m;
      m.id = jm.value("id", (int64_t)0);
      if (jm.contains("umid"))
        m.umid = jm["umid"].is_string() ? jm["umid"].get<std::string>()
                                        : std::to_string(jm["umid"].get<int64_t>());
      m.title = jm.value("title", "");
      m.body = jm.value("message", "");
      m.app = jm.value("app", "");
      m.url = jm.value("url", "");
      m.url_title = jm.value("url_title", "");
      m.sound = jm.value("sound", "");
      m.icon = jm.value("icon", "");
      m.receipt = jm.value("receipt", "");
      m.priority = jm.value("priority", 0);
      m.date = (std::time_t)jm.value("date", (int64_t)0);
      m.acked = jm.value("acked", 0) != 0;
      out.push_back(std::move(m));
    }
    Result r;
    r.ok = true;
    return r;
  } catch (const std::exception& e) {
    Result r;
    r.error = std::string("message fetch: bad JSON: ") + e.what();
    return r;
  }
}

Result ack_highest(const std::string& secret, const std::string& device_id,
                   int64_t highest_id) {
  auto resp = http::post_form(
      std::string(API) + "/devices/" + device_id + "/update_highest_message.json",
      {{"secret", secret}, {"message", std::to_string(highest_id)}});
  if (!resp.ok()) return api_fail(resp, "ack");
  Result r;
  r.ok = true;
  return r;
}

// ---------------------------------------------------------------------------
// Websocket worker

void Client::start(std::string secret, std::string device_id, Callbacks cb) {
  stop();
  secret_ = std::move(secret);
  device_id_ = std::move(device_id);
  cb_ = std::move(cb);
  stop_ = false;
  thread_ = std::thread([this] { run(); });
}

void Client::stop() {
  stop_ = true;
  if (thread_.joinable()) thread_.join();
}

void Client::fetch_and_deliver() {
  std::vector<Message> msgs;
  auto r = fetch_messages(secret_, device_id_, msgs);
  if (!r.ok) {
    logx::write("worker: " + r.error);
    return;
  }
  if (msgs.empty()) return;
  int64_t highest = 0;
  for (const auto& m : msgs) highest = std::max(highest, m.id);
  // Ack BEFORE display: Pushover re-delivers everything un-acked, and a
  // display failure must not turn into an infinite re-notification loop.
  auto ar = ack_highest(secret_, device_id_, highest);
  if (!ar.ok) logx::write("worker: " + ar.error);
  // TODO: priority-2 messages additionally need POST /1/receipts/<receipt>/acknowledge.json
  if (cb_.on_messages) cb_.on_messages(std::move(msgs));
}

void Client::run() {
  using namespace std::chrono;
  int backoff = 5;  // seconds; doubles to 300 on repeated failures

  while (!stop_) {
    // Catch up on anything queued while we were offline.
    fetch_and_deliver();

    CURL* ws = curl_easy_init();
    if (!ws) return;
    curl_easy_setopt(ws, CURLOPT_URL, "wss://client.pushover.net/push");
    curl_easy_setopt(ws, CURLOPT_CONNECT_ONLY, 2L);  // websocket mode
    curl_easy_setopt(ws, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(ws, CURLOPT_USERAGENT, "flnotify/0.1");

    bool fatal = false;
    if (curl_easy_perform(ws) == CURLE_OK) {
      std::string hello = "login:" + device_id_ + ":" + secret_ + "\n";
      size_t sent = 0;
      if (curl_ws_send(ws, hello.data(), hello.size(), &sent, 0, CURLWS_TEXT) ==
          CURLE_OK) {
        if (cb_.on_state) cb_.on_state("connected");
        logx::write("worker: websocket connected");
        backoff = 5;

        curl_socket_t sock = CURL_SOCKET_BAD;
        curl_easy_getinfo(ws, CURLINFO_ACTIVESOCKET, &sock);
        auto last_frame = steady_clock::now();

        while (!stop_) {
          fd_set rfds;
          FD_ZERO(&rfds);
          FD_SET(sock, &rfds);
          timeval tv{1, 0};
          select((int)sock + 1, &rfds, nullptr, nullptr, &tv);
          if (stop_) break;

          char buf[64];
          size_t rlen = 0;
          const curl_ws_frame* meta = nullptr;
          CURLcode rc = curl_ws_recv(ws, buf, sizeof buf, &rlen, &meta);
          if (rc == CURLE_AGAIN) {
            // No keepalive ('#' every ~30s) for 60s means a dead connection.
            if (steady_clock::now() - last_frame > seconds(60)) {
              logx::write("worker: keepalive timeout, reconnecting");
              break;
            }
            continue;
          }
          if (rc != CURLE_OK) {
            logx::write(std::string("worker: ws recv error: ") +
                        curl_easy_strerror(rc));
            break;
          }
          last_frame = steady_clock::now();
          bool reconnect = false;
          for (size_t i = 0; i < rlen && !fatal && !reconnect; ++i) {
            switch (buf[i]) {
              case '#':  // keepalive
                break;
              case '!':  // new message(s) waiting
                fetch_and_deliver();
                break;
              case 'R':  // server requests reconnect
                logx::write("worker: server requested reconnect");
                reconnect = true;
                break;
              case 'E':  // permanent error: credentials no longer valid
                logx::write("worker: auth error frame, stopping");
                if (cb_.on_auth_failed) cb_.on_auth_failed();
                fatal = true;
                break;
              case 'A':  // session replaced by another login
                logx::write("worker: session replaced, stopping");
                if (cb_.on_replaced) cb_.on_replaced();
                fatal = true;
                break;
            }
          }
          if (fatal || reconnect) break;
        }
      }
    }
    curl_easy_cleanup(ws);
    if (fatal || stop_) return;

    if (cb_.on_state)
      cb_.on_state("reconnecting in " + std::to_string(backoff) + "s");
    for (int i = 0; i < backoff && !stop_; ++i)
      std::this_thread::sleep_for(seconds(1));
    backoff = std::min(backoff * 2, 300);
  }
}

}  // namespace pushover
