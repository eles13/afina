#include <afina/concurrency/Executor.h>
namespace Afina {
namespace Concurrency {
void perform(Executor *executor) {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(executor->mutex);
            if (executor->state != Executor::State::kRun)
                break;
            auto time = std::chrono::system_clock::now() + std::chrono::milliseconds(executor->idle_time);
            while ((executor->tasks.empty()) && (executor->state == Executor::State::kRun)) {
                if (executor->empty_condition.wait_until(lock, time) == std::cv_status::timeout &&
                    executor->threads + executor->freeth > executor->low_watermark) {
                    executor->state = Executor::State::kStopping;
                    break;
                }
            }
            if(executor->state!=Executor::State::kRun){
              break;
            }
            if (executor->tasks.empty()){
                continue;
            }
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
            if (executor->state == Executor::State::kStopping && executor->tasks.size() == 0) {
                executor->state = Executor::State::kStopped;
                executor->stop.notify_all();
                break;
            }
        }
    }
    if(executor->state!=State::kStopped && executor->threads == 0){
      executor->state = kStopped;
    }
}

void Executor::Stop(bool await) {
    std::unique_lock<std::mutex> lock(mutex);
    if (state == State::kRun) {
        state = State::kStopping;
    }
    if (await && (threads > 0) && state == State::kStopping) {
        stop.wait(lock, [&]() { return threads == 0; });
    } else
    if (threads == 0)
    {
        state = State::kStopped;
    }
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
