#include "xxd.h"

namespace mutils {

static void xxd_offset(char* out, uint32_t offset) {
  sprintf(out, "%08x:", offset);
}

static void xxd_hex(char* out, const uint8_t* data, uint32_t size) {
  uint32_t ioff;
  uint32_t ooff{0};
  for (ioff = 0; ioff < 16; ioff += 2) {
    if (ioff >= size) {
      out[ooff++] = ' ';
      out[ooff++] = ' ';
      out[ooff++] = ' ';
      out[ooff++] = ' ';
      out[ooff++] = ' ';
    } else if (ioff + 1 >= size) {
      sprintf(out + ooff, " %02x  ", data[ioff]);
      ooff += 5;
    } else {
      sprintf(out + ooff, " %02x%02x", data[ioff], data[ioff + 1]);
      ooff += 5;
    }
  }
}

static void xxd_ascii(char* out, const uint8_t* data, uint32_t size) {
  uint32_t ioff;
  uint32_t ooff{0};
  out[ooff++] = ' ';
  out[ooff++] = ' ';
  for (ioff = 0; ioff < 16; ++ioff) {
    if (ioff < size) {
      auto c = data[ioff];
      if (c > 31 && c < 127)
        sprintf(out + ooff, "%c", c);
      else
        out[ooff] = '.';
    } else
      out[ooff] = ' ';
    ++ooff;
  }
}

static void xxd_line(char* out, uint32_t offset, const uint8_t* data, uint32_t size) {
  xxd_offset(out, offset);
  xxd_hex(out + 9, data, size);
  xxd_ascii(out + 49, data, size);
  out[67] = '\0';
}

void xxd(const void* data, uint32_t size, xxd_cb cb) {
  char str[72];
  uint32_t off{0};
  uint32_t lsz;
  auto ptr = reinterpret_cast<const uint8_t*>(data);
  static constexpr uint32_t BYTES_PER_LINE = 16;
  while (off < size) {
    auto remain = size - off;
    if (remain > BYTES_PER_LINE)
      lsz = BYTES_PER_LINE;
    else
      lsz = remain;
    xxd_line(str, off, ptr + off, lsz);
    cb(str);
    off += lsz;
  }
}

}
