#pragma once

#include <stdint.h>
#include <memory>
#include <list>
#include "caps.h"

namespace mutils {

class PipelineGear {
public:
  virtual ~PipelineGear() = default;

  virtual void setup() = 0;

  virtual void teardown() = 0;

  virtual const char* getName() const = 0;

  virtual void process(const void* in) = 0;

  virtual void getOutputParams(rokid::Caps& out) {
    out.clear();
  }

  void setParent(PipelineGear* parent) {
    parentGear = parent;
  }

  PipelineGear* getParent() {
    return parentGear;
  }

  // 先调用自身teardown
  // 再调用parentGear eraseOutput
  void destroy() {
    teardown();
    if (parentGear != nullptr) {
      parentGear->eraseOutput(this, true);
      parentGear = nullptr;
    }
  }

  // setup, 再加入outputs
  void addOutput(PipelineGear* output, bool weak = false) {
    output->setParent(this);
    output->setup();
    if (weak)
      weakOutputs.push_back(output);
    else
      outputs.push_back(output);
  }

  // 从outputs中删除
  // 如果outputs空了, 调用自身destroy
  void eraseOutput(PipelineGear* ptr, bool des = false) {
    ptr->setParent(nullptr);
    if (eraseOutput(weakOutputs, ptr))
      return;
    eraseOutput(outputs, ptr);
    if (des && outputs.empty())
      destroy();
  }

  bool hasOutput() const {
    return !outputs.empty();
  }

  static void link(std::vector<PipelineGear*>& gears) {
    if (gears.empty())
      return;
    for (size_t i = 1; i < gears.size(); ++i) {
      auto gear = gears[i];
      auto pg = gears[i - 1];
      if (gear->getParent() == nullptr)
        pg->addOutput(gear);
    }
    gears[0]->setup();
  }

protected:
  void pipelineOutput(const void* data) {
    auto it = outputs.begin();
    while (it != outputs.end()) {
      (*it)->process(data);
      ++it;
    }

    it = weakOutputs.begin();
    while (it != weakOutputs.end()) {
      (*it)->process(data);
      ++it;
    }
  }

  bool eraseOutput(std::list<PipelineGear*>& gears,
      PipelineGear* ptr) {
    auto it = gears.begin();
    while (it != gears.end()) {
      if (*it == ptr) {
        gears.erase(it);
        return true;
      }
      ++it;
    }
    return false;
  }

protected:
  std::list<PipelineGear*> outputs;
  PipelineGear* parentGear;
  std::list<PipelineGear*> weakOutputs;
};

} // namespace mutils
