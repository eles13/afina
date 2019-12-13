#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor;
void perform(Executor *execute);
class Executor {
  enum class State {
    // Threadpool is fully operational, tasks could be added and get executed
    kRun,

    // Threadpool is on the way to be shutdown, no ned task could be added, but
    // existing will be
    // completed as requested
    kStopping,

    // Threadppol is stopped
    kStopped
  };

public:
  Executor(std::string name, int size, size_t low, size_t hight, size_t max,
           size_t time)
      : low_watermark(low), hight_watermark(hight), max_queue_size(max),
        idle_time(time), freeth(0), threads(0){};
  ~Executor(){};

  /**
   * Signal thread pool to stop, it will stop accepting new jobs and close
   * threads just after each become free. All enqueued jobs will be complete.
   *
   * In case if await flag is true, call won't return until all background jobs
   * are done and all threads are stopped
   */
  void Stop(bool await = false);
  void Start();
  /**
   * Add function to be executed on the threadpool. Method returns true in case
   * if task has been placed onto execution queue, i.e scheduled for execution
   * and false otherwise.
   *
   * That function doesn't wait for function result. Function could always be
   * written in a way to notify caller about execution finished by itself
   */
  template <typename F, typename... Types>
  bool Execute(F &&func, Types... args) {
    // Prepare "task"
    auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

    std::unique_lock<std::mutex> lock(this->mutex);
    if (state != State::kRun || tasks.size() >= max_queue_size) {
      return false;
    }
    // Enqueue new task
    tasks.push_back(exec);
    if (threads < hight_watermark && freeth == 0) {
      std::thread t(&perform, this);
      t.detach();
      freeth++;
    }
    empty_condition.notify_one();
    return true;
  }

private:
  // No copy/move/assign allowed
  Executor(const Executor &);            // = delete;
  Executor(Executor &&);                 // = delete;
  Executor &operator=(const Executor &); // = delete;
  Executor &operator=(Executor &&);      // = delete;

  /**
   * Main function that all pool threads are running. It polls internal task
   * queue and execute tasks
   */
  friend void perform(Executor *executor);

  /**
   * Mutex to protect state below from concurrent modification
   */
  std::mutex mutex;

  /**
   * Conditional variable to await new data in case of empty queue
   */
  std::condition_variable empty_condition;

  /**
   * Vector of actual threads that perorm execution
   */
  size_t threads;

  /**
   * Task queue
   */
  std::deque<std::function<void()>> tasks;

  /**
   * Flag to stop bg threads
   */
  State state;
  size_t low_watermark;
  size_t hight_watermark;
  size_t max_queue_size;
  size_t idle_time;
  size_t working;
  size_t freeth;
  std::condition_variable stop;
};
// void perform(Executor* ex);
} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
