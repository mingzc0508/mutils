#pragma once

#include <functional>

namespace mutils {

class CircleQueueBase {
public:
  /// \brief 传入队列内存, 初始化队列数据结构
  ///
  /// 数据结构
  /// uint32_t totalBytes - 总字节数
  /// uint32_t capacity - 最大元素数
  /// uint32_t writePos - 新数据写入的位置, 永远增长, 队列元素索引值 = writePos % capacity
  /// uint32_t extraSize
  /// uint8_t[extraSize] extra - 额外数据区, 由外部使用, 队列本身不会修改此区内数据
  /// uint8_t[unitSize * capacity] elements - 元素数据区
  void setMemory(void* mem, uint32_t usize, uint32_t esize, uint32_t ucount) {
    auto p = setMemoryIn(mem);
    *totalBytes = usize * ucount + esize + 16;
    *capacity = ucount;
    *writePos = 0;
    *extraSize = esize;
    units = p + esize;
    unitSize = usize;
    unitCount = ucount;
  }

  void setMemory(void* mem) {
    auto p = setMemoryIn(mem);
    units = p + (*extraSize);
    unitSize = (*totalBytes - 16 - *extraSize) / *capacity;
    unitCount = *capacity;
  }

  const void* getExtra() const {
    return extra;
  }

  void* getExtra() {
    return extra;
  }

  void* getMemory(uint32_t* bytes) {
    if (bytes)
      *bytes = *totalBytes;
    return totalBytes;
  }

  uint32_t getMemorySize() const {
    if (totalBytes)
      return *totalBytes;
    return 0;
  }

  void reset() {
    unitSize = 0;
    unitCount = 0;
    extraSize = 0;
    totalBytes = nullptr;
    capacity = nullptr;
    writePos = nullptr;
    extra = nullptr;
    units = nullptr;
  }

  bool ready() const {
    return totalBytes != nullptr;
  }

private:
  uint8_t* setMemoryIn(void* mem) {
    uint8_t* p = (uint8_t*)mem;
    totalBytes = (uint32_t*)p;
    p += sizeof(uint32_t);
    capacity = (uint32_t*)p;
    p += sizeof(uint32_t);
    writePos = (uint32_t*)p;
    p += sizeof(uint32_t);
    extraSize = (uint32_t*)p;
    p += sizeof(uint32_t);
    extra = p;
    return p;
  }

protected:
  uint32_t unitSize{0};
  uint32_t unitCount{0};
  uint32_t* totalBytes{nullptr};
  uint32_t* capacity{nullptr};
  volatile uint32_t* writePos{nullptr};
  uint32_t* extraSize;
  void* extra{nullptr};
  uint8_t* units{nullptr};
};

/// \brief 共享内存环形队列(写)
class CircleQueueWriter : public CircleQueueBase {
public:
  static uint32_t memBytes(uint32_t usize, uint32_t ucount, uint32_t esize) {
    return 16 + esize + usize * ucount;
  }

  typedef std::function<void(void*)> WriteAction;
  void write(WriteAction action) {
    uint32_t off = (*writePos % unitCount) * unitSize;
    action(units + off);
    auto pos = *writePos;
    *writePos = pos + 1;
  }

  void* getWritePointer() {
    uint32_t off = (*writePos % unitCount) * unitSize;
    return units + off;
  }

  void writeNext() {
    auto pos = *writePos;
    *writePos = pos + 1;
  }
};

class CircleQueueReader : public CircleQueueBase {
public:
  void setMemory(void* mem) {
    CircleQueueBase::setMemory(mem);
    readPos = *writePos;
    maxRead = unitCount * 3 / 4;
    if (maxRead == 0)
      maxRead = 1;
  }

  /// \brief 读取队列最后一个元素
  ///        即使队列没有新元素加入, 也可以一直读到最后一个元素
  typedef std::function<void(const void*)> ReadAction;
  bool readLast(ReadAction action) {
    auto pos = *writePos;
    if (pos == 0)
      return false;
    uint32_t off = ((pos - 1) % unitCount) * unitSize;
    action(units + off);
    return true;
  }

  void* readLast() {
    auto pos = *writePos;
    if (pos == 0)
      return nullptr;
    uint32_t off = ((pos - 1) % unitCount) * unitSize;
    return units + off;
  }

  /// \brief 读取队列最后N个元素
  ///        读取后消耗, 再次调用不会再读取到, 直到新元素加入
  typedef std::function<void(const void*, uint32_t, uint32_t)> ReadAllAction;
  uint32_t readAll(ReadAllAction action, uint32_t outSize) {
    auto wpos = *writePos;
    auto count = wpos - readPos;
    if (count > maxRead)
      count = maxRead;
    if (count > outSize)
      count = outSize;
    auto rpos = wpos - count;
    uint32_t i;
    for (i = 0; i < count; ++i) {
      auto idx = rpos % unitCount;
      action(units + idx * unitSize, i, count);
      ++rpos;
    }
    readPos = wpos;
    return count;
  }

  void reset() {
    CircleQueueBase::reset();
    readPos = 0;
    maxRead = 0;
  }

  void clear() {
    readPos = *writePos;
  }

private:
  uint32_t readPos{0};
  uint32_t maxRead{0};
};

} // namespace mutils
