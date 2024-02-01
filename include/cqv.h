#pragma once

#include <functional>

namespace mutils {

class CircleQueueBase {
public:
  CircleQueueBase() {
    unitSize = 0;
    extraSize = 0;
  }

  /// \brief 构造函数
  ///
  /// 构造函数并不实际分配队列内存, 此时队列还不可用, 需要调用init
  ///
  /// \param usize 队列中每元素占用字节数
  /// \param esize 额外数据字节数
  CircleQueueBase(uint32_t usize, uint32_t esize) {
    init(usize, esize);
  }

  virtual ~CircleQueueBase() = default;

  void init(uint32_t usize, uint32_t esize) {
    unitSize = (usize + 7) & (~7);
    extraSize = (esize + 7) & (~7);
  }

  /// \brief 传入队列内存, 初始化队列数据结构
  ///
  /// 数据结构
  /// uint32_t totalBytes - 总字节数
  /// uint32_t capacity - 最大元素数
  /// uint32_t writePos - 新数据写入的位置, 永远增长, 队列元素索引值 = writePos % capacity
  /// uint32_t pad
  /// uint8_t[extraSize] extra - 额外数据区, 由外部使用, 队列本身不会修改此区内数据
  /// uint8_t[unitSize * capacity] elements - 元素数据区
  virtual void setMemory(void* mem) {
    uint8_t* p = (uint8_t*)mem;
    totalBytes = (uint32_t*)p;
    p += sizeof(uint32_t);
    capacity = (uint32_t*)p;
    p += sizeof(uint32_t);
    writePos = (uint32_t*)p;
    p += sizeof(uint32_t);
    p += sizeof(uint32_t); // 8-bytes align
    extra = p;
    p += extraSize;
    units = p;
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

protected:
  uint32_t unitSize;
  uint32_t unitCount{0};
  uint32_t extraSize;
  uint32_t* totalBytes{nullptr};
  uint32_t* capacity{nullptr};
  volatile uint32_t* writePos{nullptr};
  void* extra{nullptr};
  uint8_t* units{nullptr};
};

/// \brief 共享内存环形队列(写)
class CircleQueueWriter : public CircleQueueBase {
public:
  CircleQueueWriter() {
    memoryBytes = 0;
  }

  /// \param ucount 队列中元素最大个数
  CircleQueueWriter(uint32_t usize, uint32_t esize, uint32_t ucount)
    : CircleQueueBase(usize, esize) {
    unitCount = ucount;
    memoryBytes = unitSize * unitCount + extraSize + sizeof(uint32_t) * 4;
  }

  virtual ~CircleQueueWriter() = default;

  uint32_t memBytes() const {
    return memoryBytes;
  }

  void init(uint32_t usize, uint32_t esize, uint32_t ucount) {
    CircleQueueBase::init(usize, esize);
    unitCount = ucount;
    memoryBytes = unitSize * unitCount + extraSize + sizeof(uint32_t) * 4;
  }

  void setMemory(void* mem) {
    CircleQueueBase::setMemory(mem);
    *totalBytes = memoryBytes;
    *capacity = unitCount;
    *writePos = 0;
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

private:
  uint32_t memoryBytes;
};

class CircleQueueReader : public CircleQueueBase {
public:
  CircleQueueReader() {}

  CircleQueueReader(uint32_t usize, uint32_t esize)
    : CircleQueueBase(usize, esize) {}

  virtual ~CircleQueueReader() = default;

  void setMemory(void* mem) {
    CircleQueueBase::setMemory(mem);
    unitCount = *capacity;
    readPos = *writePos;
    maxRead = unitCount * 3 / 4;
  }

  typedef std::function<void(const void*)> ReadAction;
  bool read(ReadAction action) {
    auto pos = *writePos;
    if (pos == 0)
      return false;
    uint32_t off = ((pos - 1) % unitCount) * unitSize;
    action(units + off);
    return true;
  }

  void* read() {
    auto pos = *writePos;
    if (pos == 0)
      return nullptr;
    uint32_t off = ((pos - 1) % unitCount) * unitSize;
    return units + off;
  }

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

private:
  uint32_t readPos{0};
  uint32_t maxRead{0};
};

} // namespace mutils
