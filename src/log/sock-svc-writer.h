#pragma once

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <list>
#include <mutex>
#include <thread>
#include <chrono>
#include "sock-svc.h"

using namespace std;
using namespace mutils;

class SocketServiceWriter : public RLogWriter {
public:
  bool init(const void* arg) {
    if (arg == nullptr)
      return false;
    auto uri = reinterpret_cast<const char*>(arg);
    SocketService::Builder builder;
    builder.setTag("rlog");
    builder.addUri(uri);
    builder.setAsync(true);
    builder.setConnectionCallback(std::bind(
          &SocketServiceWriter::sockOnConnected,
          this,
          placeholders::_1,
          placeholders::_2));
    auto svc = builder.build();
    if (!svc->start()) {
      delete svc;
      return false;
    }
    sockService = svc;
    return true;
  }

  void destroy() {
    if (sockService) {
      sockService->stop();
      delete sockService;
      sockService = nullptr;
    }
    sockets_mutex.lock();
    auto it = cli_sockets.begin();
    while (it != cli_sockets.end()) {
      ::close(*it);
      ++it;
    }
    cli_sockets.clear();
    sockets_mutex.unlock();
  }

  bool write(const char *data, uint32_t size) {
    lock_guard<mutex> locker(sockets_mutex);
    if (size == 0)
      return true;
    auto it = cli_sockets.begin();
    ssize_t r;
    while (it != cli_sockets.end()) {
      r = ::write(*it, data, size);
      if (r <= 0) {
        auto dit = it;
        ++it;
        ::close(*dit);
        cli_sockets.erase(dit);
        continue;
      }
      ++it;
    }
    return true;
  }

private:
  void sockOnConnected(int32_t fd, bool st) {
    lock_guard<mutex> locker{sockets_mutex};
    if (st) {
      set_write_timeout(fd, write_timeout);
      cli_sockets.push_back(fd);
    }
  }

  static void set_write_timeout(int sock, uint32_t timeout) {
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  }

private:
  SocketService* sockService{nullptr};
  std::list<int> cli_sockets;
  std::mutex sockets_mutex;
  uint32_t write_timeout{800};
};
