#pragma once

namespace mutils {

class PathSplit {
public:
  static void split(const std::string& pathname, std::string& path,
      std::string& base, std::string& ext) {
    split(pathname.c_str(), path, base, ext);
  }

  static void split(const char* pathname, std::string& path,
      std::string& base, std::string& ext) {
    uint32_t i{0};
    int32_t lastSlash{-1};
    int32_t lastDot{-1};
    while (pathname[i] != '\0') {
      if (pathname[i] == '/')
        lastSlash = i;
      if (pathname[i] == '.')
        lastDot = i;
      ++i;
    }
    auto slen = i;
    uint32_t baseStart;
    if (lastSlash < 0) {
      path.clear();
      baseStart = 0;
    } else {
      path.assign(pathname, lastSlash + 1);
      baseStart = lastSlash + 1;
    }
    if (lastDot < 0 || lastDot == baseStart) {
      // 未找到'.'或文件名以'.'开头, 无扩展名
      ext.clear();
      base.assign(pathname + baseStart, slen - baseStart);
    } else {
      base.assign(pathname + baseStart, lastDot - baseStart);
      ext.assign(pathname + lastDot, slen - lastDot);
    }
  }
};

} // namespace mutils
