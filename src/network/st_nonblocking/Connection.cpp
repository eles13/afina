#include "Connection.h"

#include <iostream>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    _logger->info("offset on descriptor {}", _socket);
    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    command_to_execute.reset();
    argument_for_command.resize(0);
    parser.Reset();
    alive = true;
}

// See Connection.h
void Connection::OnError() {
    _logger->info("Error on descriptor {}", _socket);
    alive = false;
}

// See Connection.h
void Connection::OnClose() {
    _logger->info("Close on descriptor {}", _socket);
    alive = false;
}

// See Connection.h
void Connection::DoRead() {
    try {
        int for_read = -1;
        while ((for_read = read(_socket, client_buffer + readed_bytes, sizeof(client_buffer) - readed_bytes)) > 0) {
            _logger->debug("Got {} bytes from socket", readed_bytes);
            readed_bytes += for_read;
            // Single block of data readed from the socket could trigger inside actions a multiple times,
            // for example:
            // - read#0: [<command1 offset>]
            // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (readed_bytes > 0) {
                _logger->debug("Process {} bytes", readed_bytes);
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(client_buffer, readed_bytes, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(client_buffer, client_buffer + parsed, readed_bytes - parsed);
                        readed_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", readed_bytes, arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));
                    argument_for_command.append(client_buffer, to_read);

                    std::memmove(client_buffer, client_buffer + to_read, readed_bytes - to_read);
                    arg_remains -= to_read;
                    readed_bytes -= to_read;
                }

                // Thre is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("offset command execution");

                    std::string result;
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    // Send response
                    result += "\r\n";
                    bool nw = results.empty();
                    results.push_back(result);

                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                    if (nw) {
                        _event.events = EPOLLOUT | EPOLLRDHUP | EPOLLERR;
                    }
                }
            } // while (readed_bytes)
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
        alive = false;
    }
}

// See Connection.h
void Connection::DoWrite() {
    assert(!results.empty());
    try {
        struct iovec vecs[results.size()];
        size_t i = 0;
        for (auto result : results) {
            vecs[i].iov_len = result.size();
            vecs[i].iov_base = &(result[0]);
            i++;
        }
        vecs[0].iov_base = (char *)(vecs[0].iov_base) + offset;
        vecs[0].iov_len -= offset;
        int done;
        if ((done = writev(_socket, vecs, results.size())) <= 0) {
            throw std::runtime_error(std::string(strerror(errno)));
        }
        offset += done;
        auto result_it = results.begin();
        for (auto result : results) {
            if (offset < result.size()) {
                break;
            }
            offset -= result.size();
            result_it++;
        }
        results.erase(results.begin(), result_it);
        if (results.empty()) {
            _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to writing to connection on descriptor {}: {} \n", _socket, ex.what());
        alive = false;
    }
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
