#include <afina/concurrency/Executor.h>
#include <iostream>
namespace Afina {
namespace Concurrency {
void perform(Executor *executor) {
  while (executor->state == Executor::State::kRun) {
    std::function<void()> task;

    {
      std::unique_lock<std::mutex> lock(executor->mutex);
      auto time = std::chrono::system_clock::now() +
                  std::chrono::milliseconds(executor->idle_time);
      while ((executor->tasks.empty()) &&
             (executor->state == Executor::State::kRun)) {
        if (executor->empty_condition.wait_until(lock, time) ==
                std::cv_status::timeout &&
            executor->threads + executor->freeth > executor->low_watermark) {
          return;
        } else {
          executor->empty_condition.wait(lock);
        }
      }
      if (executor->tasks.empty())
        continue;
      task = executor->tasks.front();
      executor->tasks.pop_front();
      executor->threads++;
      executor->freeth--;
    }
    try {
      task();
    } catch (...) {
      std::terminate();
    }
    {
      std::unique_lock<std::mutex> lock(executor->mutex);
      executor->freeth++;
      executor->threads--;
      if (executor->state == Executor::State::kStopping &&
          executor->tasks.size() == 0) {
        executor->state = Executor::State::kStopped;
        executor->stop.notify_all();
      }
    }
  }
  std::cout<<"out\n";
}

void Executor::Stop(bool await) {
  std::unique_lock<std::mutex> lock(mutex);
  if (state == State::kRun) {
    state = State::kStopping;
  }
  if (await && (threads > 0)) {
    stop.wait(lock, [&]() { return threads + freeth == 0; });
  } else {
    state = State::kStopped;
  }
  std::cout<<"stop\n";
}

void Executor::Start() {
  state = State::kRun;
  std::unique_lock<std::mutex> lock(mutex);
  for (size_t i = 0; i < low_watermark; i++) {
    std::thread t(&(perform), this);
    t.detach();
    freeth++;
  }
}

} // namespace Concurrency
} // namespace Afina
