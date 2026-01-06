#pragma once

#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string>
#include <chrono>
#include "uri.h"
#ifdef __APPLE__
#include <errno.h>
#endif

namespace mutils {

class SocketClient {
public:
  static int32_t connect(const std::string& uri, uint32_t timeout) {
    Uri urip;
    int32_t sock;
    if (!urip.parse(uri))
      return -1;
    uint32_t sockType;
    if (urip.scheme == "unix") {
      sock = createUnixSocket();
      sockType = 0;
    } else if (urip.scheme == "tcp") {
      sock = createTcpSocket();
      sockType = 1;
    } else
      return -1;
    if (sock < 0)
      return -1;
    auto fl = fcntl(sock, F_GETFL);
    fl |= O_NONBLOCK;
    fcntl(sock, F_SETFL, fl);
    bool r = false;
    if (sockType == 0)
      r = connectUnix(sock, urip);
    else if (sockType == 1)
      r = connectTcp(sock, urip);
    if (!r) {
      if (errno != EINPROGRESS)
        return -1;
      auto conntp = std::chrono::steady_clock::now();
      uint32_t elapsed{0};
      fd_set fdset;
      int sr;
      do {
        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);
        if (timeout) {
          struct timeval tv;
          tv.tv_sec = (timeout - elapsed) / 1000;
          tv.tv_usec = (timeout - elapsed) % 1000 * 1000;
          sr = select(sock + 1, nullptr, &fdset, nullptr, &tv);
        } else {
          sr = select(sock + 1, nullptr, &fdset, nullptr, nullptr);
        }
        if (sr >= 0)
          break;
        if (errno != EINTR)
          break;
        auto nowtp = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(nowtp - conntp).count();
      } while (elapsed < timeout);
      if (sr <= 0) {
        ::close(sock);
        return -1;
      }
    }
    fl &= (~O_NONBLOCK);
    fcntl(sock, F_SETFL, fl);
    return sock;
  }

private:
  static int createUnixSocket() {
#ifdef __APPLE__
    auto fd = socket(AF_UNIX, SOCK_STREAM, 0);
#else
    auto fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
#endif
    if (fd < 0)
      return -1;
#ifdef __APPLE__
    auto f = fcntl(fd, F_GETFD);
    f |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, f);
#endif
    return fd;
  }

  static int createTcpSocket() {
#ifdef __APPLE__
    auto fd = socket(AF_INET, SOCK_STREAM, 0);
#else
    auto fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
#endif
    if (fd < 0)
      return -1;
#ifdef __APPLE__
    auto f = fcntl(fd, F_GETFD);
    f |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, f);
#endif
    return fd;
  }

  static bool connectUnix(int fd, const Uri& urip) {
    if (urip.path.empty())
      return false;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    uint32_t addrlen;
#ifdef __APPLE__
    strncpy(addr.sun_path, urip.path.c_str(), sizeof(addr.sun_path));
    addrlen = offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path) + 1;
#else
    if (urip.path[0] == '/') {
      strncpy(addr.sun_path, urip.path.c_str(), sizeof(addr.sun_path));
      addrlen = offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path) + 1;
    } else {
      addr.sun_path[0] = '\0';
      strncpy(addr.sun_path + 1, urip.path.c_str(), sizeof(addr.sun_path) - 1);
      addrlen = offsetof(sockaddr_un, sun_path) + strlen(addr.sun_path + 1) + 2;
    }
#endif
    return ::connect(fd, (sockaddr *)&addr, addrlen) == 0;
  }

  static bool connectTcp(int fd, const Uri& urip) {
    struct sockaddr_in addr;
    struct hostent *hp;
    hp = gethostbyname(urip.host.c_str());
    if (hp == nullptr)
      return false;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, hp->h_addr_list[0], sizeof(addr.sin_addr));
    addr.sin_port = htons(urip.port);
    return ::connect(fd, (sockaddr *)&addr, sizeof(addr)) == 0;
  }
};

} // namespace mutils
