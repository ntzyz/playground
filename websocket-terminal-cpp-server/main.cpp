#include <algorithm>
#include <cctype>
#include <cstdbool>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <functional>

#include <arpa/inet.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <getopt.h>

#include "base64.hpp"
#include "sha1.hpp"
#include "pty.hpp"
#include "frontend.h"

inline std::string &string_trim(std::string &s) {
  if (s.empty()) {
    return s;
  }

  s.erase(0, s.find_first_not_of("\r\n "));
  s.erase(s.find_last_not_of("\r\n ") + 1);

  return s;
}

inline uint32_t mask_gen() {
  static uint32_t mask = 0xdeadbeef;
  return mask ++;
}

enum websocket_connection_stage_t {
  handshaking,
  established,
  terminated,
  unknown,
};

struct http_request_t {
  enum {
    GET,
    POST,
    PUT,
    OPTIONS,
    DELETE,
    UNKNOWN // etc
  } request_method;
  std::string request_path;
  std::string http_version;
  std::map<std::string, std::vector<std::string>> headers;

  http_request_t(const uint8_t *const raw_request,
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

  void parse_http_method(std::stringstream &ss) {
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

  void parse_http_path(std::stringstream &ss) {
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

  void parse_http_version(std::stringstream &ss) {
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

  void parse_http_headers(std::stringstream &ss) {
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
};

struct websocket_frame_t {
  using opcode_t = enum {
    CONTINUATION_FRAME = 0x00,
    TEXT_FRAME = 0x01,
    BINARY_FRAME = 0x02,
    CONNECTION_CLOSE_FRAME = 0x08,
    PING_FRAME = 0x09,
    PONG_FRAME = 0x0A,
  };

  bool fin;
  opcode_t opcode;
  bool mask;
  uint8_t mask_key[4] = { 0, 0, 0, 0 };
  std::vector<uint8_t> payload;

  bool debug = false;

  websocket_frame_t(std::unique_ptr<uint8_t[]> buffer, size_t buffer_size) {
    size_t offset = 0;

    fin = (buffer[offset] >> 7) & 0x01;
    opcode = static_cast<opcode_t>(buffer[offset] & 0x1F);

    offset ++;

    mask = buffer[offset] >> 7;
    uint64_t payload_len = static_cast<uint64_t>(buffer[offset] & 0x7f);

    if (payload_len == 126) {
      payload_len = static_cast<uint64_t>(buffer[offset + 1] << 8) |
                    static_cast<uint64_t>(buffer[offset + 2]);
      offset += 2;
    }

    if (payload_len == 127) {
      payload_len = (static_cast<uint64_t>(buffer[offset + 1]) << 56) |
                    (static_cast<uint64_t>(buffer[offset + 2]) << 48) |
                    (static_cast<uint64_t>(buffer[offset + 3]) << 40) |
                    (static_cast<uint64_t>(buffer[offset + 4]) << 32) |
                    (static_cast<uint64_t>(buffer[offset + 5]) << 24) |
                    (static_cast<uint64_t>(buffer[offset + 6]) << 16) |
                    (static_cast<uint64_t>(buffer[offset + 7]) << 8) |
                    static_cast<uint64_t>(buffer[offset + 8]);
      offset += 8;
    }

    offset ++;

    if (mask) {
      for (size_t i = 0; i != 4; i++) {
        mask_key[i] = buffer[offset + i];
      }

      offset += 4;
    }

    payload = std::vector<uint8_t>(buffer.get() + offset, buffer.get() + offset + payload_len);

    if (mask) {
      for (size_t i = 0; i != payload.size(); i++) {
        payload[i] ^= mask_key[i % 4];
      }
    }

    if (debug) {
      print_frame_info();
    }
  }

  websocket_frame_t() {
    fin = true;
    opcode = opcode_t::TEXT_FRAME;
    mask = true;
    uint32_t raw_mask = mask_gen();

    mask_key[0] = raw_mask & 0xff000000;
    mask_key[1] = raw_mask & 0x00ff0000;
    mask_key[2] = raw_mask & 0x0000ff00;
    mask_key[3] = raw_mask & 0x000000ff;
  }

  void print_frame_info() {
    printf("FIN:\t\t%s\n", fin ? "true" : "false");

    switch (opcode) {
    case CONTINUATION_FRAME:
      puts("OPCODE:\t\tCONTINUATION_FRAME");
      break;
    case TEXT_FRAME:
      puts("OPCODE:\t\tTEXT_FRAME");
      break;
    case BINARY_FRAME:
      puts("OPCODE:\t\tBINARY_FRAME");
      break;
    case CONNECTION_CLOSE_FRAME:
      puts("OPCODE:\t\tCONNECTION_CLOSE_FRAME");
      break;
    case PING_FRAME:
      puts("OPCODE:\t\tPING_FRAME");
      break;
    case PONG_FRAME:
      puts("OPCODE:\t\tPONG_FRAME");
      break;
    }

    printf("MASK:\t\t%s\n", mask ? "true" : "false");
    if (mask) {
      printf("  MASK_KEY:\t%02X%02X%02X%02X\n", mask_key[0], mask_key[1], mask_key[2], mask_key[3]);
    }

    printf("PAYLOAD_LEN:\t%ld\n", payload.size());
    printf("PAYLOAD:\n");

    for (size_t i = 0; i != payload.size(); i++) {
      printf("%02X ", payload[i]);

      if (i != 0 && i % 8 == 0) {
        if (i % 16 == 0) {
          putchar('\n');
          continue;
        }
        putchar(' ');
      }
    }

    puts("\n");
  }

  std::unique_ptr<uint8_t[]> to_raw_frame(size_t *frame_size) {
    ssize_t expected_frame_size = 0;
    
    // 假设现在 payload 的长度为 0，则在有 mask 的情况下，报文头部的长度应该是固定的，是：
    // (FIN, RSV1 2 3, OPCODE)  => 1 byte
    // (MASK, PAYLOAD)          => 1 byte
    // (MASK KEY)               => 4 bytes
    // (PAYLOAD)                => 0 byte
    // 的和，也就是 5 byte
    expected_frame_size = 6;

    if (126 <= payload.size() && payload.size() <= 65535) {
      // 该情况下，(MASK, PAYLOAD) 后续跟了一个 uint16_t 表示真正的载荷大小，因此预期的头部长度多了 2byte
      expected_frame_size += 2;
    }
    if (payload.size() > 65535) {
      // 类似上面，但是是 uint64_t
      expected_frame_size += 8;
    }

    if (!mask) {
      expected_frame_size -= 4;
    }

    // 然后再算上载荷的大小，就是预期的帧大小了
    expected_frame_size += payload.size();
    
    size_t offset = 0;
    auto raw_frame = std::make_unique<uint8_t[]>(expected_frame_size);
    std::fill(raw_frame.get(), raw_frame.get() + expected_frame_size, 0);
    *frame_size = expected_frame_size;

    // FIN, RSV1, RSV2, RSV3, OPCODE
    if (fin) {
      raw_frame[offset] |= 0b10000000;
    }
    raw_frame[offset] |= opcode;

    offset ++;

    // MASK
    if (mask) {
      raw_frame[offset] |= 0b10000000;
    }

    // PAYLOAD_LENGTH
    if (payload.size() < 126) {
      raw_frame[offset] |= payload.size();
      offset += 1;
    } if (126 <= payload.size() && payload.size() <= 65535) {
      raw_frame[offset] |= 126;
      raw_frame[offset + 1] = (payload.size() >> 8);
      raw_frame[offset + 2] = payload.size() & 0xff;
      offset += 3;
    } if (payload.size() > 65535) {
      raw_frame[offset] |= 127;
      raw_frame[offset + 1] = (payload.size() >> 56) & 0xff;
      raw_frame[offset + 2] = (payload.size() >> 48) & 0xff;
      raw_frame[offset + 3] = (payload.size() >> 40) & 0xff;
      raw_frame[offset + 4] = (payload.size() >> 32) & 0xff;
      raw_frame[offset + 5] = (payload.size() >> 24) & 0xff;
      raw_frame[offset + 6] = (payload.size() >> 16) & 0xff;
      raw_frame[offset + 7] = (payload.size() >> 8) & 0xff;
      raw_frame[offset + 8] = payload.size() & 0xff;
      offset += 9;
    }

    // MASKING KEY
    if (mask) {
      raw_frame[offset] = mask_key[0];
      raw_frame[offset + 1] = mask_key[1];
      raw_frame[offset + 2] = mask_key[2];
      raw_frame[offset + 3] = mask_key[3];
      offset += 4;
    }

    // PAYLOAD
    for (size_t i = 0; i != payload.size(); i++) {
      raw_frame[offset + i] = payload[i];
      
      if (mask) {
        raw_frame[offset + i] ^= mask_key[i % 4];
      }
    }

    return raw_frame;
  }
};

struct http_response_t {
  int status_code;
  std::string status_text;
  std::map<std::string, std::vector<std::string>> headers;
  std::string body;

  http_response_t(const int _in_status_code) : status_code(_in_status_code) {
    http_status_code_to_status_text();
    initialize_response_headers();
  };

  void http_status_code_to_status_text() {
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

  void initialize_response_headers() {
    append_header_date();
    append_header_connection();
    append_header_server();
  }

  void append_header_date() {
    char temp[1000];
    time_t now = time(nullptr);
    struct tm tm = *gmtime(&now);

    strftime(temp, sizeof(temp), "%a, %d %b %Y %H:%M:%S %Z", &tm);

    headers["date"] = std::vector<std::string>({temp});
  }

  void append_header_connection() {
    if (status_code == 101) {
      headers["connection"] = std::vector<std::string>({"upgrade"});
      headers["upgrade"] = std::vector<std::string>({"websocket"});
    } else {
      headers["connection"] = std::vector<std::string>({"close"});
    }
  }

  void append_header_server() {
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

  std::string to_response() {
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
};

struct websocket_context_t {
  websocket_connection_stage_t stage = websocket_connection_stage_t::unknown;
  int client_fd = -1;

  uint8_t *received_buffer = nullptr;
  ssize_t received_buffer_capacity = -1;
  ssize_t received_buffer_used = -1;
  bool enable_builtin_webpages = false;
  pty_t pty;
  std::function<void(void)> regist_pty_session;

  void append_buffer(uint8_t *buffer, ssize_t buffer_size) {
    if (buffer_size <= 0) {
      printf("session terminating\n");
      stage = websocket_connection_stage_t::terminated;
      return;
    }

    if (received_buffer == nullptr) {
      received_buffer_capacity = 1024;
      received_buffer_used = 0;
      received_buffer = new uint8_t[received_buffer_capacity];
    }

    if (buffer_size + received_buffer_used > received_buffer_capacity) {
      ssize_t new_buffer_capacity = received_buffer_capacity * 2;
      uint8_t *new_buffer = new uint8_t[new_buffer_capacity];

      std::copy(received_buffer, received_buffer + received_buffer_used,
                new_buffer);

      delete[] received_buffer;
      received_buffer = new_buffer;
      received_buffer_capacity = new_buffer_capacity;
    }

    auto append_buffer_begin = received_buffer + received_buffer_used;
    std::copy(buffer, buffer + buffer_size, append_buffer_begin);

    received_buffer_used += buffer_size;
  }

  void process() {
    if (this->stage == websocket_connection_stage_t::handshaking) {
      this->process_handshake();
      return;
    }

    if (this->stage == websocket_connection_stage_t::terminated) {
      return;
    }

    this->process_data();
  }

  void process_handshake() {
    if (received_buffer[received_buffer_used - 4] == '\r' &&
        received_buffer[received_buffer_used - 3] == '\n' &&
        received_buffer[received_buffer_used - 2] == '\r' &&
        received_buffer[received_buffer_used - 1] == '\n') {
      http_request_t request(received_buffer, received_buffer_used);

      const auto ws_sec_key_header_it =
          request.headers.find("sec-websocket-key");
      const bool is_websocket = ws_sec_key_header_it != request.headers.end() &&
                                (ws_sec_key_header_it->second).size() == 1;

      http_response_t response(is_websocket ? 101 : 200);

      if (is_websocket) {
        if (regist_pty_session) {
          pty.spawn();
          regist_pty_session();
          regist_pty_session = std::function<void(void)>{};
        }

        const auto key = (ws_sec_key_header_it->second)[0];

        printf("Sec-WebSocket-Key sent from client is %s\n", key.c_str());

        SHA1 websocket_sec_hash;

        websocket_sec_hash.update(
            (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").c_str());

        const auto hash = websocket_sec_hash.final();
        uint8_t hash_binary[20];

        for (size_t i = 0; i != hash.size(); i += 2) {
          char temp[] = {
              hash[i],
              hash[i + 1],
              '\0',
          };

          sscanf(temp, "%02hhX", hash_binary + i / 2);
        }

        std::string hash_base64 =
            base64::encode(std::vector<uint8_t>(hash_binary, hash_binary + 20));

        response.headers["Sec-WebSocket-Accept"] =
            std::vector<std::string>({hash_base64});
      } else {
        if (request.request_path == "/" && enable_builtin_webpages) {
          std::string file_content(frontend_html, frontend_html + frontend_html_len);

          response.headers["content-type"] =
              std::vector<std::string>({"text/html; charset=utf-8"});
          response.headers["content-length"] = std::vector<std::string>({std::to_string(file_content.size())});
          response.body = file_content;
        } else {
          response.headers["content-length"] = std::vector<std::string>({"0"});
          response.headers["content-type"] =
              std::vector<std::string>({"text/plain; charset=utf-8"});
          response.status_code = 404;
          response.status_text = "Not Found";
        }
      }

      const auto str = response.to_response();
      send(client_fd, str.c_str(), str.size(), 0);

      if (is_websocket) {
        stage = established;
        received_buffer_used = 0;
      } else {
        stage = terminated;
        close(client_fd);
      }
    }
  }

  ssize_t next_valid_ws_frame_size() {
    ssize_t expected_buffer_size = 2;
    size_t cursor = 1;

    if (received_buffer_used < expected_buffer_size) {
      return -1;
    }

    uint8_t mask = received_buffer[cursor] >> 7;
    uint64_t payload_len =
        static_cast<uint64_t>(received_buffer[cursor] & 0x7f);

    if (payload_len == 126) {
      // two bytes for extended payload length (uint16_t in big endian)
      expected_buffer_size += 2;

      if (received_buffer_used < expected_buffer_size) {
        return -1;
      }

      payload_len = static_cast<uint64_t>(received_buffer[cursor + 1] << 8) |
                    static_cast<uint64_t>(received_buffer[cursor + 2]);
      cursor += 2;
    }

    if (payload_len == 127) {
      // eight bytes for extended payload length (uint64_t in big endian)
      expected_buffer_size += 8;

      if (received_buffer_used < expected_buffer_size) {
        return -1;
      }

      payload_len = (static_cast<uint64_t>(received_buffer[cursor + 1]) << 56) |
                    (static_cast<uint64_t>(received_buffer[cursor + 2]) << 48) |
                    (static_cast<uint64_t>(received_buffer[cursor + 3]) << 40) |
                    (static_cast<uint64_t>(received_buffer[cursor + 4]) << 32) |
                    (static_cast<uint64_t>(received_buffer[cursor + 5]) << 24) |
                    (static_cast<uint64_t>(received_buffer[cursor + 6]) << 16) |
                    (static_cast<uint64_t>(received_buffer[cursor + 7]) << 8) |
                    static_cast<uint64_t>(received_buffer[cursor + 8]);
      cursor += 8;
    }

    if (mask) {
      // four bytes for optional mask key
      expected_buffer_size += 4;

      if (received_buffer_used < expected_buffer_size) {
        return -1;
      }

      cursor += 4;
    }

    expected_buffer_size += payload_len;

    if (received_buffer_used < expected_buffer_size) {
      return -1;
    }

    return expected_buffer_size;
  }

  void process_data() {
    auto frame_size = next_valid_ws_frame_size();
    auto raw_frame = std::make_unique<uint8_t[]>(frame_size);

    std::copy(received_buffer, received_buffer + frame_size, raw_frame.get());

    for (ssize_t i = frame_size; i != received_buffer_used; i++) {
      received_buffer[i - frame_size] = received_buffer[i];
    }
    
    received_buffer_used -= frame_size;

    websocket_frame_t frame(std::move(raw_frame), frame_size);

    if (frame.opcode == websocket_frame_t::opcode_t::CONNECTION_CLOSE_FRAME) {
      stage = terminated;
      close(client_fd);
      return;
    }

    auto buffer = std::make_unique<uint8_t>(frame.payload.size());
    std::copy(frame.payload.begin(), frame.payload.end(), buffer.get());

    write(pty.master_fd, buffer.get(), frame.payload.size());
  }
};

int create_server(uint16_t port, bool localhost = true) {
  int server_fd = 0;
  struct sockaddr_in server;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    exit(1);
  }

  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr.s_addr = htonl(localhost ? INADDR_LOOPBACK : INADDR_ANY);

  int opt_val = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);
  if (bind(server_fd, reinterpret_cast<struct sockaddr *>(&server),
           sizeof(server)) < 0) {
    perror("bind");
    exit(1);
  }

  opt_val = 1;
  if (ioctl(server_fd, FIONBIO, (char *)&opt_val) < 0) {
    perror("ioctl");
    exit(1);
  }

  if (listen(server_fd, SOMAXCONN) < 0) {
    perror("listen");
    exit(1);
  }

  printf("[INFO] Server startted on %s:%d\n", localhost ? "127.0.0.1" : "0.0.0.0", port);
  return server_fd;
}

int main(int argc, char *argv[]) {
  const int POOL_TIMEOUT = 10;
  const int MAX_FDS = 512;
  int server_fd;
  int nfds;
  struct pollfd fds[MAX_FDS];
  std::map<int, std::shared_ptr<websocket_context_t>> sessions;
  std::map<int, std::shared_ptr<websocket_context_t>> pty_sessions;
  int bind_port = 2333;
  int opt;
  bool enable_builtin_webpages = false;
  bool allow_remote = false;

  while ((opt = getopt(argc, argv, "p:w::r::")) != -1) {
    switch (opt) {
    case 'p':
      bind_port = atoi(optarg);
      break;
    case 'w':
      enable_builtin_webpages = true;
      break;
    case 'r':
      allow_remote = true;
      break;
    default: /* '?' */
      fprintf(stderr, "Usage: %s [-p port] [-w] [-r]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  server_fd = create_server(bind_port, !allow_remote);
  nfds = 1;

  memset(fds, 0, sizeof(fds));
  fds[0].fd = server_fd;
  fds[0].events = POLLIN;

  for (;;) {
    int poll_result = poll(fds, nfds, POOL_TIMEOUT);

    if (poll_result < 0) {
      perror("poll");
      exit(1);
    }

    if (poll_result == 0) {
      continue;
    }

    int current_fds_size = nfds;
    bool shrink_fds_required = false;

    for (int i = 0; i != current_fds_size; i++) {
      if (fds[i].revents == 0) {
        continue;
      }

      if (fds[i].revents != POLLIN && fds[i].revents != POLLHUP) {
        perror("received poll event is unexpected.");
        exit(1);
      }

      if (fds[i].fd == server_fd) {
        // New connection comming. accept it.
        int client_fd = accept(server_fd, NULL, NULL);

        if (client_fd < 0 && errno != EWOULDBLOCK) {
          perror("accept");
        }

        int opt_val = 1;
        if (ioctl(client_fd, FIONBIO, (char *)&opt_val) < 0) {
          perror("ioctl");
          exit(1);
        }

        fds[nfds].fd = client_fd;
        fds[nfds].events = POLLIN;
        nfds++;

        // add it to sessions
        auto session = std::make_shared<websocket_context_t>();
        session->enable_builtin_webpages = enable_builtin_webpages;
        session->client_fd = client_fd;
        session->stage = websocket_connection_stage_t::handshaking;

        sessions[client_fd] = session;
        session->regist_pty_session = [&fds, &nfds, &pty_sessions, session]() {
          fds[nfds].fd = session->pty.master_fd;
          fds[nfds].events = POLLIN;
          nfds++;
          pty_sessions[session->pty.master_fd] = session;
        };

        printf("new connection accepted, current nfds = %d\n", nfds);
      } else if (sessions.find(fds[i].fd) != sessions.end() && fds[i].revents == POLLIN) {
        // some client send me some data, read and handle it.
        ssize_t buffer_size = 1048576;
        uint8_t buffer[buffer_size];

        for (;;) {
          int client_fd = fds[i].fd;
          int read_result = recv(client_fd, buffer, buffer_size, 0);

          if (read_result < 0) {
            if (errno != EWOULDBLOCK) {
              perror("recv");
              fds[i].fd = -1;
              shrink_fds_required = true;
            }

            break;
          }

          auto session = sessions[client_fd];

          session->append_buffer(buffer, read_result);
          session->process();

          if (session->stage == terminated || session->stage == unknown) {
            // erase it from websocket sessions
            fds[i].fd = -1;
            sessions.erase(sessions.find(client_fd));

            // also erase it from pty sessions
            if (pty_sessions.find(session->pty.master_fd) != pty_sessions.end()) {
              pty_sessions.erase(pty_sessions.find(session->pty.master_fd));
            }

            for (ssize_t j = 0; j != nfds; j++) {
              if (fds[j].fd == session->pty.master_fd) {
                fds[j].fd = -1;
              }
            }

            shrink_fds_required = true;
            break;
          }
        }
      } else if (pty_sessions.find(fds[i].fd) != pty_sessions.end()) {
        auto session = pty_sessions[fds[i].fd];
        ssize_t buffer_size = 1024;
        uint8_t buffer[buffer_size];

        if (fds[i].revents == POLLHUP) {
          // erase it from pty sessions
          pty_sessions.erase(pty_sessions.find(fds[i].fd));
          fds[i].fd = -1;

          // also erase it from ws sessions
          sessions.erase(sessions.find(session->client_fd));
          close(session->client_fd);

          for (ssize_t j = 0; j != nfds; j++) {
            if (fds[j].fd == session->client_fd) {
              fds[j].fd = -1;
            }
          }

          shrink_fds_required = true;
          break;
        } else for (;;) {
          int pty_master_fd = fds[i].fd;
          int read_result = read(pty_master_fd, buffer, buffer_size);

          if (read_result < 0) {
            if (errno != EWOULDBLOCK) {
              perror("read");
              fds[i].fd = -1;
              shrink_fds_required = true;
            }

            break;
          }

          websocket_frame_t response_frame;
          response_frame.payload = std::vector<uint8_t>(buffer, buffer + read_result);

          size_t response_frame_size = 0;
          auto response_data = response_frame.to_raw_frame(&response_frame_size);

          send(session->client_fd, response_data.get(), response_frame_size, 0);
        }
      }
    }

    if (shrink_fds_required) {
      shrink_fds_required = false;

      for (int i = 0; i != nfds; i++) {
        if (fds[i].fd == -1) {
          for (int j = i; j < nfds - 1; j++) {
            fds[j] = fds[j + 1];
          }

          i--, nfds--;
          printf("one fd closed, current nfds = %d\n", nfds);
        }
      }
    }
  }
}
