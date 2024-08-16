#pragma once

#include <inttypes.h>

namespace mutils {

class ByteOrder {
public:
  // little-endian
  static void readShortLE(const uint8_t* data, uint16_t* out) {
    out[0] = data[0] | (data[1] << 8);
  }

  static void writeShortLE(uint8_t* data, uint16_t v) {
    data[0] = v & 0xff;
    data[1] = v >> 8;
  }

  static void readIntLE(const uint8_t* data, uint32_t* out) {
    out[0] = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
  }

  static void writeIntLE(uint8_t* data, uint32_t v) {
    data[0] = v & 0xff;
    data[1] = (v >> 8) & 0xff;
    data[2] = (v >> 16) & 0xff;
    data[3] = v >> 24;
  }
};

} // namespace mutils
