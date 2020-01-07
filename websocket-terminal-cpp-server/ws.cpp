#include "ws.hpp"

static inline uint32_t mask_gen() {
  static uint32_t mask = 0xdeadbeef;
  return mask ++;
}

websocket_frame_t::websocket_frame_t(std::unique_ptr<uint8_t[]> buffer, size_t buffer_size) {
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

websocket_frame_t::websocket_frame_t() {
  fin = true;
  opcode = opcode_t::TEXT_FRAME;
  mask = true;
  uint32_t raw_mask = mask_gen();

  mask_key[0] = raw_mask & 0xff000000;
  mask_key[1] = raw_mask & 0x00ff0000;
  mask_key[2] = raw_mask & 0x0000ff00;
  mask_key[3] = raw_mask & 0x000000ff;
}

void websocket_frame_t::print_frame_info() {
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

std::unique_ptr<uint8_t[]> websocket_frame_t::to_raw_frame(size_t *frame_size) {
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

void websocket_context_t::append_buffer(uint8_t *buffer, ssize_t buffer_size) {
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

void websocket_context_t::process() {
  if (this->stage == websocket_connection_stage_t::handshaking) {
    this->process_handshake();
    return;
  }

  if (this->stage == websocket_connection_stage_t::terminated) {
    return;
  }

  this->process_data();
}

void websocket_context_t::process_handshake() {
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

ssize_t websocket_context_t::next_valid_ws_frame_size() {
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

void websocket_context_t::process_data() {
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

websocket_context_t::~websocket_context_t() {
  if (received_buffer != nullptr) {
    delete[] received_buffer;
  }
}