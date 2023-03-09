#pragma once

#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef __APPLE__
#include <fcntl.h>
#include <poll.h>
#include <map>
#else
#include <sys/epoll.h>
#include <set>
#endif
#include <string>
#include <vector>
#include <functional>
#include <thread>
#include "uri.h"
#include "rlog.h"

#define SOCKET_TYPE_LISTEN 1
#ifndef __APPLE__
#define MAX_EPOLL_EVENTS 128
#endif

namespace mutils {

enum class SocketServiceStatus {
  STOPPED = 0,
  RUNNING
};

class SocketService {
public:
  /// \param 1 int32_t sockfd
  /// \param 2 bool true=connect  false=disconnect
  typedef std::function<void(int32_t, bool)> ConnectionCallback;
  /// \param 1 int32_t sockfd
  /// \return true=read success, false=read failed
  typedef std::function<bool(int32_t)> ReadCallback;

  class Builder {
  public:
    Builder() {
      svc = new SocketService();
    }

    ~Builder() {
      if (svc != nullptr)
        delete svc;
    }

    Builder& setTag(const std::string& tag) {
      svc->tag_ = tag;
      svc->ITAG = svc->tag_.c_str();
      return *this;
    }

    Builder& addUri(const std::string& uri) {
      svc->uris.push_back(uri);
      return *this;
    }

    Builder& setConnectionCallback(ConnectionCallback cb) {
      svc->connectionCallback = cb;
      return *this;
    }

    Builder& setReadCallback(ReadCallback cb) {
      svc->readCallback = cb;
      return *this;
    }

    SocketService* build() {
      auto ret = svc;
      svc = nullptr;
      return ret;
    }

  private:
    SocketService* svc;
  };

  bool start() {
    if (uris.empty())
      return false;
#ifdef __APPLE__
    // 在非poll线程shutdown/close socket，poll不一定会返回，不同操作系统行为不一样
    // 所以需要在poll队列中加入一个特殊的socket，用于在stop时唤醒poll线程
    // epoll没有这种问题
    addWakeupPollSocket();
#else
    epollfd = epoll_create1(EPOLL_CLOEXEC);
    if (epollfd < 0) {
      KLOGI(ITAG, "epoll_create failed: %s", strerror(errno));
      return false;
    }
#endif

    Uri urip;
    auto it = uris.begin();
    uint32_t listenSocks{0};
    while (it != uris.end()) {
      if (!urip.parse(it->c_str())) {
        KLOGW(ITAG, "uri %s parse failed", it->c_str());
        ++it;
        continue;
      }
      if (urip.scheme == "unix") {
        if (!listenUnix(urip))
          continue;
      } else if(urip.scheme == "tcp") {
        if (!listenTcp(urip))
          continue;
      } else {
        ++it;
        continue;
      }
      ++listenSocks;
      KLOGI(ITAG, "socket service listening %s", it->c_str());
      ++it;
    }
    if (listenSocks == 0)
      return false;
    status = SocketServiceStatus::RUNNING;
    serverRunThread = std::thread(serverRunTask);
    return true;
  }

  void stop() {
    if (status == SocketServiceStatus::STOPPED)
      return;
    status = SocketServiceStatus::STOPPED;
#ifdef __APPLE__
    wakeupPoll();
#else
    auto it = sockets.begin();
    ::shutdown(*it, SHUT_RDWR);
#endif
    if (serverRunThread.joinable())
      serverRunThread.join();
  }

private:
  SocketService() {
    serverRunTask = [this]() {
      while (status == SocketServiceStatus::RUNNING) {
        if (!doPoll())
          break;
        doReadAccept();
      }
      closeSockets();
    };
  }

  bool listenUnix(Uri& urip) {
#ifdef __APPLE__
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
#else
    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
#endif
    if (fd < 0) {
      KLOGE(ITAG, "socket create failed: %s", strerror(errno));
      return false;
    }
#ifdef __APPLE__
    auto f = fcntl(fd, F_GETFD);
    f |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, f);
#endif
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    uint32_t abslen = urip.path.length() + 1;
    if (abslen > sizeof(addr.sun_path))
      abslen = sizeof(addr.sun_path);
#ifdef __APPLE__
    unlink(urip.path.c_str());
    strncpy(addr.sun_path, urip.path.c_str(), abslen);
#else
    addr.sun_path[0] = '\0';
    memcpy(addr.sun_path + 1, urip.path.data(), abslen - 1);
#endif
    abslen += offsetof(sockaddr_un, sun_path);
    if (::bind(fd, (sockaddr *)&addr, abslen) < 0) {
      ::close(fd);
      KLOGE(ITAG, "socket bind failed: %s", strerror(errno));
      return false;
    }
    listen(fd, 10);
    addSocket(fd, SOCKET_TYPE_LISTEN);
    return true;
  }

  bool listenTcp(Uri& urip) {
#ifdef __APPLE__
    int fd = socket(AF_INET, SOCK_STREAM, 0);
#else
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
#endif
    if (fd < 0) {
      KLOGE(ITAG, "socket create failed: %s", strerror(errno));
      return false;
    }
#ifdef __APPLE__
    auto f = fcntl(fd, F_GETFD);
    f |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, f);
#endif
    int v = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
    struct sockaddr_in addr;
    struct hostent *hp;
    hp = gethostbyname(urip.host.c_str());
    if (hp == nullptr) {
      KLOGE(ITAG, "gethostbyname failed for host %s: %s",
          urip.host.c_str(), strerror(errno));
      ::close(fd);
      return false;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, hp->h_addr_list[0], sizeof(addr.sin_addr));
    addr.sin_port = htons(urip.port);
    if (::bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
      ::close(fd);
      KLOGE(ITAG, "socket bind failed: %s", strerror(errno));
      return false;
    }
    listen(fd, 10);
    addSocket(fd, SOCKET_TYPE_LISTEN);
    return true;
  }

  void addSocket(int fd, int type) {
#ifdef __APPLE__
    struct pollfd tmp;
    tmp.fd = fd;
    tmp.events = POLLIN;
    tmp.revents = 0;
    allfds.push_back(tmp);
    sockets.insert(make_pair(fd, type));
#else
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.u64 = makeEPollData(fd, type);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
    sockets.insert(fd);
#endif
  }

  uint64_t makeEPollData(int32_t fd, uint32_t type) {
    return ((uint64_t)type << 32) | (uint32_t)fd;
  }

  int32_t fdOfEPollData(uint64_t data) {
    return (int32_t)((data << 32) >> 32);
  }

  uint32_t typeOfEPollData(uint64_t data) {
    return (uint32_t)(data >> 32);
  }

  void closeSockets() {
#ifdef __APPLE__
    auto it = sockets.begin();
    while (it != sockets.end()) {
      ::close(it->first);
      ++it;
    }
    sockets.clear();
#else
    ::close(epollfd);
    epollfd = -1;
    auto it = sockets.begin();
    while (it != sockets.end()) {
      ::close(*it);
      ++it;
    }
    sockets.clear();
#endif
  }

#ifdef __APPLE__
  void addWakeupPollSocket() {
    Uri urip;
    urip.parse(wakeupPollUri.c_str());
    listenUnix(urip);
  }

  void wakeupPoll() {
    /** TODO: new socket and connect to wakeupPollUri
    ClientSocketAdapter adap{options.bufsize};
    Uri urip;
    urip.parse(wakeupPollUri.c_str());
    adap.connect(urip, 10);
    */
  }
#endif

  bool doPoll() {
    while (true) {
#ifdef __APPLE__
      auto r = poll(allfds.data(), allfds.size(), -1);
#else
      auto r = epoll_wait(epollfd, epollEvents, MAX_EPOLL_EVENTS, -1);
      polloutNum = r;
#endif
      if (r < 0) {
        if (errno == EINTR)
          continue;
        if (errno == EAGAIN) {
          sleep(1);
          continue;
        }
        KLOGE(ITAG, "poll failed: %s", strerror(errno));
        return false;
      }
      break;
    }
    return true;
  }

  void doReadAccept() {
#ifdef __APPLE__
    vector<struct pollfd> rfds;
    auto it = allfds.begin();
    while (it != allfds.end()) {
      if (it->revents)
        rfds.push_back(*it);
      ++it;
    }
    it = rfds.begin();
    map<int, int>::iterator sit;
    while (it != rfds.end()) {
      sit = sockets.find(it->fd);
      assert(sit != sockets.end());
      doReadAccept(sit->first, sit->second);
      ++it;
    }
#else
    for (int32_t i = 0; i < polloutNum; ++i) {
      auto fd = fdOfEPollData(epollEvents[i].data.u64);
      auto type = typeOfEPollData(epollEvents[i].data.u64);
      doReadAccept(fd, type);
    }
#endif
  }

  void doReadAccept(int fd, int type) {
    if (type == SOCKET_TYPE_LISTEN) {
      if (!doAccept(fd))
        closeSocket(fd);
    } else {
      doRead(fd);
    }
  }

  bool doAccept(int fd) {
#ifdef __APPLE__
    auto newfd = accept(fd, nullptr, nullptr);
    if (newfd < 0) {
      KLOGW(ITAG, "accept failed: %s", strerror(errno));
      return false;
    }
    auto f = fcntl(fd, F_GETFD);
    f |= FD_CLOEXEC;
    fcntl(fd, F_SETFD, f);
#else
    auto newfd = accept4(fd, nullptr, nullptr, SOCK_CLOEXEC);
    if (newfd < 0) {
      KLOGW(ITAG, "accept failed: %s", strerror(errno));
      return false;
    }
#endif
    addSocket(newfd, 0);
    KLOGD(ITAG, "accept new connection %d", newfd);
    if (connectionCallback != nullptr)
      connectionCallback(newfd, true);
    return true;
  }

  void doRead(int fd) {
    if (readCallback == nullptr || !readCallback(fd)) {
      if (connectionCallback != nullptr)
        connectionCallback(fd, false);
      closeSocket(fd);
    }
  }

  void closeSocket(int fd) {
#ifdef __APPLE__
    auto it = allfds.begin();
    while (it != allfds.end()) {
      if (it->fd == fd) {
        allfds.erase(it);
        break;
      }
      ++it;
    }
    sockets.erase(fd);
#else
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, nullptr);
    sockets.erase(fd);
#endif
    ::close(fd);
  }

private:
  std::vector<std::string> uris;
  std::string tag_{"mutils.SocketService"};
  const char* ITAG;
#ifdef __APPLE__
  // key: fd, value: socketType
  map<int, int> sockets;
  vector<struct pollfd> allfds;
  string wakeupPollUri{"unix:flora-svc-special.sock"};
#else
  int epollfd{-1};
  struct epoll_event epollEvents[MAX_EPOLL_EVENTS];
  int32_t polloutNum{0};
  set<int32_t> sockets;
#endif
  std::function<void()> serverRunTask;
  std::thread serverRunThread;
  SocketServiceStatus status{SocketServiceStatus::STOPPED};
  ConnectionCallback connectionCallback;
  ReadCallback readCallback;
};

} // namespace mutils
