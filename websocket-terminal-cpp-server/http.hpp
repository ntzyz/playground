#pragma once

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>

#include <cstring>
#include <cstdint>

#include <sys/utsname.h>

struct http_request_t {
  enum {
    GET, POST, PUT, OPTIONS,
    DELETE, UNKNOWN // etc
  } request_method;
  std::string request_path;
  std::string http_version;
  std::map<std::string, std::vector<std::string>> headers;

  http_request_t(const uint8_t *const, const ssize_t);

  void parse_http_method(std::stringstream &);
  void parse_http_path(std::stringstream &);
  void parse_http_version(std::stringstream &);
  void parse_http_headers(std::stringstream &);
};

struct http_response_t {
  int status_code;
  std::string status_text;
  std::map<std::string, std::vector<std::string>> headers;
  std::string body;

  http_response_t(const int);

  void http_status_code_to_status_text();
  void initialize_response_headers();
  void append_header_date();
  void append_header_connection();
  void append_header_server();
  std::string to_response();
};