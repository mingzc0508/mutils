#include <atomic>
#include "gtest/gtest.h"
#include "thr-pool.h"

using namespace std;
using namespace std::chrono;

TEST(ThreadPool, doTask) {
  ThreadPool pool;
  uint32_t taskDone{0};
  uint32_t taskNum{100};
  pool.init(1);
  uint32_t i;
  for (i = 0; i < taskNum; ++i) {
    pool.push([&taskDone]() {
      ++taskDone;
    });
  }
  pool.finish();
  EXPECT_EQ(taskDone, taskNum);
}

TEST(ThreadPool, queueTask) {
  ThreadPool pool;
  uint32_t maxThreads = 5;
  uint32_t taskNum = 10;
  uint32_t i;
  uint32_t repeat{0};
  const static uint32_t REPEAT_NUM = 3;
  struct {
    atomic<uint32_t> running{0};
    atomic<uint32_t> maxRunning{0};
    atomic<uint32_t> done{0};
  } vars;

  pool.init(maxThreads);
  while (repeat < REPEAT_NUM) {
    ++repeat;
    for (i = 0; i < taskNum; ++i) {
      pool.push([&vars]() {
        ++vars.running;
        if (vars.maxRunning < vars.running)
          vars.maxRunning = vars.running.load();
        sleep(5);
        --vars.running;
        ++vars.done;
      });
    }
    pool.finish();
    EXPECT_EQ(vars.maxRunning.load(), maxThreads);
    EXPECT_EQ(vars.done.load(), taskNum);
    EXPECT_EQ(vars.running.load(), 0);
    vars.running = 0;
    vars.maxRunning = 0;
    vars.done = 0;
  }
}

TEST(ThreadPool, delayedTask) {
  ThreadPool thrPool{1};
  struct {
    steady_clock::time_point pushtp;
    uint32_t delayTime = 1000;
    uint32_t done = 0;
    uint32_t delayedDone = 0;
  } vars;
  vars.pushtp = steady_clock::now();

  thrPool.push([&vars]() {
    ++vars.done;
  });
  thrPool.push([&vars]() {
    ++vars.done;
  });
  thrPool.push([&vars]() {
    ++vars.delayedDone;
  }, vars.delayTime, [&vars](ThreadPool::TaskOp op) {
    if (op == ThreadPool::TaskOp::DOING) {
      auto now = steady_clock::now();
      EXPECT_TRUE(duration_cast<milliseconds>(now - vars.pushtp).count() >= vars.delayTime);
    }
  });
  thrPool.finish(FINISH_FLAG_WAIT_DELAYED_TASK);
  EXPECT_EQ(vars.done, 2);
  EXPECT_EQ(vars.delayedDone, 1);
}

TEST(ThreadPool, discardDelayedTask) {
  ThreadPool thrPool{1};
  struct {
    steady_clock::time_point pushtp;
    uint32_t delayTime = 1000;
    uint32_t done = 0;
    uint32_t delayedDone = 0;
    uint32_t discard = 0;
  } vars;
  vars.pushtp = steady_clock::now();

  thrPool.push([&vars]() {
    ++vars.done;
  });
  thrPool.push([&vars]() {
    ++vars.done;
  });
  thrPool.push([&vars]() {
    ++vars.delayedDone;
  }, 3000, [&vars](ThreadPool::TaskOp op) {
    if (op == ThreadPool::TaskOp::DISCARD)
      ++vars.discard;
    else
      EXPECT_TRUE(false);
  });
  thrPool.push([&vars]() {
    ++vars.delayedDone;
  }, vars.delayTime, [&vars](ThreadPool::TaskOp op) {
    if (op == ThreadPool::TaskOp::DOING) {
      auto now = steady_clock::now();
      EXPECT_TRUE(duration_cast<milliseconds>(now - vars.pushtp).count() >= vars.delayTime);
    }
  });
  sleep(2);
  thrPool.finish();
  EXPECT_EQ(vars.done, 2);
  EXPECT_EQ(vars.delayedDone, 1);
  EXPECT_EQ(vars.discard, 1);
}

TEST(ThreadPool, multiDelayedTask) {
  ThreadPool thrPool{3};
  struct {
    steady_clock::time_point pushtp;
    uint32_t done0 = 0;
    uint32_t done1 = 0;
    uint32_t delayedDone0 = 0;
    uint32_t delayedDone1 = 0;
    uint32_t delayedDone2 = 0;
    uint32_t delayTime0 = 1000;
    uint32_t delayTime1 = 1500;
    uint32_t delayTime2 = 6000;
    uint32_t repeat0 = 1000;
    uint32_t repeat1 = 3;
    uint32_t delayedRepeat0 = 300;
    uint32_t delayedRepeat1 = 500;
    uint32_t delayedRepeat2 = 800;
    mutex varMutex;
  } vars;
  vars.pushtp = steady_clock::now();
  auto task0 = [&vars]() {
    lock_guard<mutex> locker(vars.varMutex);
    ++vars.done0;
  };
  auto task1 = [&vars]() {
    sleep(5);
    lock_guard<mutex> locker(vars.varMutex);
    ++vars.done1;
  };
  auto delayedTask0 = [&vars]() {
    lock_guard<mutex> locker(vars.varMutex);
    ++vars.delayedDone0;
    EXPECT_TRUE(steady_clock::now() >= vars.pushtp + milliseconds{vars.delayTime0});
  };
  auto delayedTask1 = [&vars]() {
    lock_guard<mutex> locker(vars.varMutex);
    ++vars.delayedDone1;
    EXPECT_TRUE(steady_clock::now() >= vars.pushtp + milliseconds{vars.delayTime1});
  };
  auto delayedTask2 = [&vars]() {
    lock_guard<mutex> locker(vars.varMutex);
    ++vars.delayedDone2;
    EXPECT_TRUE(steady_clock::now() >= vars.pushtp + milliseconds{vars.delayTime2});
  };
  uint32_t i;
  for (i = 0; i < vars.repeat0; ++i)
    thrPool.push(task0);
  for (i = 0; i < vars.repeat1; ++i)
    thrPool.push(task1);
  for (i = 0; i < vars.delayedRepeat0; ++i)
    thrPool.push(delayedTask0, vars.delayTime0);
  for (i = 0; i < vars.delayedRepeat1; ++i)
    thrPool.push(delayedTask1, vars.delayTime1);
  for (i = 0; i < vars.delayedRepeat2; ++i)
    thrPool.push(delayedTask2, vars.delayTime2);
  thrPool.finish(FINISH_FLAG_WAIT_DELAYED_TASK);
  EXPECT_EQ(vars.done0, vars.repeat0);
  EXPECT_EQ(vars.done1, vars.repeat1);
  EXPECT_EQ(vars.delayedDone0, vars.delayedRepeat0);
  EXPECT_EQ(vars.delayedDone1, vars.delayedRepeat1);
  EXPECT_EQ(vars.delayedDone2, vars.delayedRepeat2);
}
