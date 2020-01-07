#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include <cstring>
#include <cstdint>

#include <unistd.h>
#include <sys/socket.h>

#include "pty.hpp"
#include "http.hpp"
#include "base64.hpp"
#include "sha1.hpp"
#include "frontend.hpp"

enum websocket_connection_stage_t {
  handshaking,
  established,
  terminated,
  unknown,
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

  websocket_frame_t(std::unique_ptr<uint8_t[]>, size_t);

  websocket_frame_t();

  void print_frame_info();

  std::unique_ptr<uint8_t[]> to_raw_frame(size_t *frame_size);
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

  ~websocket_context_t();

  void append_buffer(uint8_t *buffer, ssize_t buffer_size);
  void process();
  void process_handshake();
  ssize_t next_valid_ws_frame_size();
  void process_data();
};