#include "net/http.h"

#include <curl/curl.h>

namespace {

size_t write_cb(char* data, size_t size, size_t nmemb, void* userp) {
  static_cast<std::string*>(userp)->append(data, size * nmemb);
  return size * nmemb;
}

CURL* make_handle(std::string& body, char* errbuf) {
  CURL* h = curl_easy_init();
  if (!h) return nullptr;
  curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(h, CURLOPT_WRITEDATA, &body);
  curl_easy_setopt(h, CURLOPT_ERRORBUFFER, errbuf);
  curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(h, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(h, CURLOPT_USERAGENT, "flnotify/0.1");
  return h;
}

// Takes resp by reference: the curl handle holds a pointer to resp.body,
// so resp must not be moved/copied between setup and perform.
void perform(CURL* h, char* errbuf, HttpResponse& resp) {
  CURLcode rc = curl_easy_perform(h);
  if (rc != CURLE_OK) {
    resp.status = 0;
    resp.error = errbuf[0] ? errbuf : curl_easy_strerror(rc);
  } else {
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &resp.status);
  }
  curl_easy_cleanup(h);
}

}  // namespace

namespace http {

bool global_init() {
  return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}

void global_cleanup() {
  curl_global_cleanup();
}

HttpResponse get(const std::string& url) {
  HttpResponse resp;
  char errbuf[CURL_ERROR_SIZE] = {0};
  CURL* h = make_handle(resp.body, errbuf);
  if (!h) { resp.error = "curl init failed"; return resp; }
  curl_easy_setopt(h, CURLOPT_URL, url.c_str());
  perform(h, errbuf, resp);
  return resp;
}

HttpResponse post_form(const std::string& url,
                       const std::map<std::string, std::string>& fields) {
  HttpResponse resp;
  char errbuf[CURL_ERROR_SIZE] = {0};
  CURL* h = make_handle(resp.body, errbuf);
  if (!h) { resp.error = "curl init failed"; return resp; }

  std::string form;
  for (const auto& [k, v] : fields) {
    char* esc = curl_easy_escape(h, v.c_str(), (int)v.size());
    if (!form.empty()) form += '&';
    form += k;
    form += '=';
    if (esc) { form += esc; curl_free(esc); }
  }
  curl_easy_setopt(h, CURLOPT_URL, url.c_str());
  curl_easy_setopt(h, CURLOPT_COPYPOSTFIELDS, form.c_str());
  perform(h, errbuf, resp);
  return resp;
}

}  // namespace http
