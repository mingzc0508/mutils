#pragma once

#include <string>
#include <vector>

namespace mutils {

class PathSplit {
public:
  /// \brief 将路径名拆分为 路径, 文件名, 扩展名
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

  /// \brief 路径/a/b/c/d解析为/a, /a/b, /a/b/c, /a/b/c/d, 用于递归创建目录
  static int32_t recurse(const std::string& path, std::vector<std::string>& out) {
    return recurse(path.c_str(), out);
  }

  static int32_t recurse(const char* path, std::vector<std::string>& out) {
    auto p = path;
    if (p[0] != '/')
      return -1;
    std::string tmp;
    while (true) {
      ++p;
      if (p[0] == '/' || p[0] == '\0') {
        tmp.assign(path, p - path);
        out.push_back(tmp);
      }
      if (p[0] == '\0')
        break;
    }
    return 0;
  }
};

} // namespace mutils
