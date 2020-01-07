#pragma once

struct pty_t {
  bool ready = false;
  int master_fd;

  pty_t();
  void spawn();
  ~pty_t();
};