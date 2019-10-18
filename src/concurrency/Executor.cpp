#include <afina/concurrency/Executor.h>
#include <algorithm>
namespace Afina {
namespace Concurrency
{
  void perform(Executor* executor)
  {
    while (executor->state == Executor::State::kRun)
    {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(executor->mutex);
            auto time = std::chrono::system_clock::now() + std::chrono::milliseconds(executor->idle_time);
            while ((executor->tasks.empty()) && (executor->state == Executor::State::kRun))
            {
                executor->freeth++;
                if (executor->empty_condition.wait_until(lock, time) == std::cv_status::timeout)
                {
                    if (executor->threads.size() > executor->low_watermark)
                    {
                      executor->deleteth();
                      return;
                    }
                    else
                        executor->empty_condition.wait(lock);
                }
                executor->freeth--;
            }
            if (executor->tasks.empty())
              continue;
            task = executor->tasks.front();
            executor->tasks.pop_front();
        }
        task();
    }
    {
        std::unique_lock<std::mutex> lock(executor->mutex);
        executor->deleteth();
        if (executor->tasks.empty())
          executor->stop.notify_all();
    }
  }

  void Executor::Stop(bool await)
  {
    state = State::kStopping;
    empty_condition.notify_all();
    if (await)
    {
        std::unique_lock<std::mutex> lock(mutex);
        stop.wait(lock, [&](){return threads.size() == 0; });
        while (!threads.empty())
          deleteth();
    }
    state = State::kStopped;
  }

  void Executor::Start()
  {
    state = State::kRun;
    std::unique_lock<std::mutex> lock(mutex);
    for (size_t i = 0; i < low_watermark; i++)
    {
        threads.push_back(std::thread(&perform, this));
    }
    freeth = 0;
  }

  void Executor::deleteth()
  {
    if (state == State::kStopping)
    {
      threads[0].detach();
      threads.erase(threads.begin());
    }
    else
    {
      std::thread::id id = std::this_thread::get_id();
      size_t i;
      auto it = std::find_if(threads.begin(), threads.end(), [id](std::thread thread) { return id == thread.get_id();});
      if (it != threads.end())
      {
        it->detach();
        threads.erase(it);
      }
    }
  }
}
} // namespace Afina
