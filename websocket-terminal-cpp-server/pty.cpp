#include <string>
#include <cstring>

#include "pty.hpp"
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <pty.h>
#include <termios.h>

pty_t::pty_t () {
  struct winsize winp;
  winp.ws_col = 80;
  winp.ws_row = 24;
  winp.ws_xpixel = 0;
  winp.ws_ypixel = 0;

  pid_t pid = forkpty(&master_fd, nullptr, nullptr, &winp);

  if (pid == -1) {
    perror("forkpty");
    return;
  } else if (pid == 0) { // master
    char *params[] = {
      strdup("/usr/bin/bash"),
      strdup("--login"),
      nullptr
    };

    execvp("/usr/bin/bash", params);

    perror("execvp");
  } else {
    int flags = fcntl(master_fd, F_GETFL, 0);
    fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

    puts("pts created.");
    ready = true;
  }
}

pty_t::~pty_t() {
  close(master_fd);
  puts("pts closed.");
}