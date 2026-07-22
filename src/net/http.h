#pragma once
#include <map>
#include <string>

struct HttpResponse {
  long status = 0;      // HTTP status; 0 = transport error (see error)
  std::string body;
  std::string error;    // curl error string when status == 0
  bool ok() const { return status >= 200 && status < 300; }
};

namespace http {

bool global_init();     // once at startup (curl_global_init)
void global_cleanup();

HttpResponse get(const std::string& url);
HttpResponse post_form(const std::string& url,
                       const std::map<std::string, std::string>& fields);

}  // namespace http
