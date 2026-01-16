#pragma once

#include <unistd.h>
#include "rlog.h"
#ifdef __ANDROID__
#include <android/log.h>
#endif

class AndroidWriter : public RLogWriter {
public:
  bool init(const void* arg) {
    return true;
  }

  void destroy() {
  }

  bool write(const char *data, uint32_t size) {
    return false;
  }

  int32_t raw_write(const char* file, int line, RokidLogLevel lv,
      const char* tag, const char* fmt, va_list ap) {
    if (lv < logLevel)
      return 1;
#ifdef __ANDROID__
    int prio = to_android_loglevel(lv);
    __android_log_vprint(prio, tag, fmt, ap);
    return 1;
#else
    return -1;
#endif // __ANDROID__
  }

#ifdef __ANDROID__
private:
  static int to_android_loglevel(RokidLogLevel lv) {
    static int android_loglevel[] = {
      ANDROID_LOG_VERBOSE,
      ANDROID_LOG_DEBUG,
      ANDROID_LOG_INFO,
      ANDROID_LOG_WARN,
      ANDROID_LOG_ERROR
    };
    if (lv < 0 || lv >= ROKID_LOGLEVEL_NUMBER)
      return ANDROID_LOG_DEFAULT;
    return android_loglevel[lv];
  }
#endif // __ANDROID__
};
