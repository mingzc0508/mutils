#pragma once

#include <stdint.h>
#include <string>
#include <functional>
#include <chrono>

namespace rokid {

typedef std::function<void(const std::string&, uint32_t)> TimeStatCallback;

/// \brief 耗时统计
class TimeStat {
public:
  TimeStat(const std::string& nm, TimeStatCallback cb) {
    name = nm;
    callback = cb;
    start();
  }

  ~TimeStat() {
    end();
  }

  void start() {
    starttp = std::chrono::steady_clock::now();
  }

  void end() {
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - starttp);
    if (callback != nullptr)
      callback(name, duration.count());
  }

  uint32_t getDuration() {
    return duration.count();
  }

private:
  std::string name;
  TimeStatCallback callback;
  std::chrono::steady_clock::time_point starttp;
  std::chrono::milliseconds duration;
};

} // namespace rokid
