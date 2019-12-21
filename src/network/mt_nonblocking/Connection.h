#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <afina/Storage.h>
#include <afina/execute/Command.h>
#include <afina/logging/Service.h>
#include <spdlog/logger.h>
#include <sys/epoll.h>
#include <sys/uio.h>

#include "protocol/Parser.h"
namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> ps, std::shared_ptr<spdlog::logger> log)
        : _socket(s), pStorage(ps), _logger(log) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
        _event.events = EPOLLIN;
        alive.store(false);
        offset = 0;
        for_read = 0;
    }

    inline bool isAlive() const { return alive; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class Worker;
    friend class ServerImpl;

    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> pStorage;
    std::atomic<bool> alive;
    int _socket;
    struct epoll_event _event;
    std::size_t arg_remains;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
    std::vector<std::string> results;
    size_t offset;
    std::mutex mutex;
    int readed_bytes;
    int for_read;
    char client_buffer[4096];
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
