#pragma once

#include <stdint.h>
#include <functional>

namespace mutils {

typedef std::function<void(const char*)> xxd_cb;

void xxd(const void* data, uint32_t size, xxd_cb cb);

}
