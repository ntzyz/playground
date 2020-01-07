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
#include <signal.h>

#include "http.hpp"
#include "ws.hpp"
#include "frontend.hpp"

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

      if (fds[i].revents != POLLIN && fds[i].revents != POLLHUP && errno != EWOULDBLOCK) {
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

          if (send(session->client_fd, response_data.get(), response_frame_size, MSG_NOSIGNAL) < 0) {
            // erase it from websocket sessions
            sessions.erase(sessions.find(session->client_fd));

            // also erase it from pty sessions
            if (pty_sessions.find(session->pty.master_fd) != pty_sessions.end()) {
              pty_sessions.erase(pty_sessions.find(session->pty.master_fd));
            }

            for (ssize_t j = 0; j != nfds; j++) {
              if (fds[j].fd == session->pty.master_fd || fds[j].fd == session->client_fd) {
                fds[j].fd = -1;
              }
            }

            shrink_fds_required = true;
            break;
          }
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
