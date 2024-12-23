#pragma once

namespace mutils {

class BitsMap {
public:
  ~BitsMap() {
    if (map)
      delete[] map;
  }

  void init(uint32_t count) {
    if (map) {
      delete[] map;
      map = nullptr;
    }
    bitsCount = count;
    integers = count / 32;
    if (count % 32)
      ++integers;
    bytes = integers * 4;
    if (integers) {
      map = new uint32_t[integers];
      initMapValue(count % 32);
    }
  }

  void copy(BitsMap& o) {
    if (o.integers != integers) {
      if (map) {
        delete[] map;
        map = nullptr;
      }
      integers = o.integers;
      bytes = o.bytes;
      if (o.integers)
        map = new uint32_t[o.integers];
    }
    if (o.integers)
      memcpy(map, o.map, o.bytes);
    bitsCount = o.bitsCount;
  }

  inline void set(uint32_t idx) {
    map[idx / 32] |= 1 << (idx % 32);
  }

  inline bool get(uint32_t idx) const {
    return map[idx / 32] & (1 << (idx % 32));
  }

  inline uint8_t* memory() {
    return (uint8_t*)map;
  }

  inline uint32_t bytesCount() const {
    return bytes;
  }

  inline uint32_t size() const {
    return bitsCount;
  }

  bool isAllSet() const {
    if (integers == 0)
      return false;
    for (uint32_t i = 0; i < integers; ++i) {
      if (map[i] != 0xffffffffu)
        return false;
    }
    return true;
  }

private:
  void initMapValue(uint32_t remain) {
    memset(map, 0, bytes);
    if (remain) {
      uint32_t value = 1u << remain;
      --value;
      map[integers - 1] = ~value;
    }
  }

private:
  uint32_t bitsCount{0};
  uint32_t* map{nullptr};
  uint32_t integers{0};
  uint32_t bytes{0};
};

} //namespace mutils
