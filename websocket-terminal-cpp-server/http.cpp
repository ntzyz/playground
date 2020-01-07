#include "http.hpp"

static inline std::string &string_trim(std::string &s) {
  if (s.empty()) {
    return s;
  }

  s.erase(0, s.find_first_not_of("\r\n "));
  s.erase(s.find_last_not_of("\r\n ") + 1);

  return s;
}

http_request_t::http_request_t(const uint8_t *const raw_request,
                               const ssize_t raw_request_size) {
  uint8_t *buffer = new uint8_t[raw_request_size + 1];

  std::copy(raw_request, raw_request + raw_request_size, buffer);
  buffer[raw_request_size] = '\0';

  std::stringstream ss(reinterpret_cast<char *>(buffer));

  // HTTP Method
  parse_http_method(ss);

  // HTTP Path
  parse_http_path(ss);

  // HTTP Version
  parse_http_version(ss);

  // Other HTTP Headers
  parse_http_headers(ss);

  delete[] buffer;
}

void http_request_t::parse_http_method(std::stringstream &ss) {
  std::string method;

  ss >> method;

  if (method == "GET") {
    request_method = GET;
  } else {
    fprintf(stderr, "warning: unsupported http method: %s, continue anyway\n",
            method.c_str());
    request_method = UNKNOWN;
  }
}

void http_request_t::parse_http_path(std::stringstream &ss) {
  std::string path;

  ss >> path;

  if (path[0] != '/') {
    fprintf(stderr,
            "warning: http path dose not have the leading slash: %s, "
            "continue anyway\n",
            path.c_str());
  }

  request_path = path;
}

void http_request_t::parse_http_version(std::stringstream &ss) {
  std::string version;
  const char *http_version_prefix = "HTTP/";
  ss >> version;

  if (strstr(version.c_str(), http_version_prefix) == nullptr) {
    fprintf(stderr, "warning: http version is invalid: %s, continue anyway\n",
            version.c_str());
  }

  http_version = std::string(version.begin() + strlen(http_version_prefix),
                              version.end());
}

void http_request_t::parse_http_headers(std::stringstream &ss) {
  std::string line;

  while (std::getline(ss, line)) {
    string_trim(line);

    if (line.empty()) {
      continue;
    }

    std::string key(line.begin(), line.begin() + line.find(':'));
    std::string value(line.begin() + line.find(':') + 1, line.end());

    string_trim(key);
    string_trim(value);

    std::transform(key.begin(), key.end(), key.begin(),
                    [](auto ch) { return tolower(ch); });

    auto header_map_it = headers.find(key);

    if (header_map_it == headers.end()) {
      headers[key] = std::vector<std::string>({value});
    } else {
      header_map_it->second.push_back(value);
    }
  }
}

http_response_t::http_response_t(const int _in_status_code)
  : status_code(_in_status_code) {
  http_status_code_to_status_text();
  initialize_response_headers();
};

void http_response_t::http_status_code_to_status_text() {
  const char *p = "";

  switch (status_code) {
  case 101:
    p = "Switching Protocols";
    break;
  case 200:
    p = "OK";
    break;
  case 400:
    p = "Bad Request";
    break;
  }

  status_text = std::string(p);
}

void http_response_t::initialize_response_headers() {
  append_header_date();
  append_header_connection();
  append_header_server();
}

void http_response_t::append_header_date() {
  char temp[1000];
  time_t now = time(nullptr);
  struct tm tm = *gmtime(&now);

  strftime(temp, sizeof(temp), "%a, %d %b %Y %H:%M:%S %Z", &tm);

  headers["date"] = std::vector<std::string>({temp});
}

void http_response_t::append_header_connection() {
  if (status_code == 101) {
    headers["connection"] = std::vector<std::string>({"upgrade"});
    headers["upgrade"] = std::vector<std::string>({"websocket"});
  } else {
    headers["connection"] = std::vector<std::string>({"close"});
  }
}

void http_response_t::append_header_server() {
  static bool uname_loaded = false;
  static struct utsname uname_data;
  static char server_name[1024];

  if (!uname_loaded) {
    uname(&uname_data);
    sprintf(server_name, "%s/%s", uname_data.sysname, uname_data.release);
    uname_loaded = true;
  }

  headers["server"] = std::vector<std::string>({server_name});
}

std::string http_response_t::to_response() {
  std::stringstream ss;
  ss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";

  for (auto &header : headers) {
    std::string field = header.first;

    for (auto it = field.begin(); it != field.end(); it++) {
      if (it == field.begin() || (!isalpha(*it) && it + 1 != field.end())) {
        *it = toupper(*it);
      }
    }

    for (auto &value : header.second) {
      ss << field << ": " << value << "\r\n";
    }
  }

  ss << "\r\n";
  ss << body;

  return ss.str();
}