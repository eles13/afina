#include <afina/concurrency/Executor.h>
#include <algorithm>
namespace Afina {
namespace Concurrency {
void perform(Executor *executor) {
    while (executor->state == Executor::State::kRun) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(executor->mutex);
            auto time = std::chrono::system_clock::now() + std::chrono::milliseconds(executor->idle_time);
            while ((executor->tasks.empty()) && (executor->state == Executor::State::kRun)) {
                executor->freeth++;
                if (executor->empty_condition.wait_until(lock, time) == std::cv_status::timeout && executor->threads > executor->low_watermark) {
                  executor->freeth--;
                    } else
                        executor->empty_condition.wait(lock);
                }
                executor->freeth--;
            }
            if (executor->tasks.empty())
                continue;
            task = executor->tasks.front();
            executor->tasks.pop_front();
            try{
              task();
            }
            catch(...)
            {
              std::terminate();
            }
        }
    {
        std::unique_lock<std::mutex> lock(executor->mutex);
        if (executor->tasks.empty() && executor->threads == 0)
            executor->stop.notify_all();
    }
}

void Executor::Stop(bool await) {
    state = State::kStopping;
    empty_condition.notify_all();
    if (await && (threads > 0)) {
        std::unique_lock<std::mutex> lock(mutex);
        stop.wait(lock, [&]() { return threads == 0; });
    }
    state = State::kStopped;
}

void Executor::Start() {
    state = State::kRun;
    std::unique_lock<std::mutex> lock(mutex);
    for (size_t i = 0; i < low_watermark; i++) {
      std::thread t(&(perform), this);
      t.detach();
      threads++;
    }
}

} // namespace Concurrency
} // namespace Afina
