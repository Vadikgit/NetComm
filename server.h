#pragma once

#include "common.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <algorithm>
#include <list>
#include <unordered_map>
#include <memory>

#include <mutex>
#include <thread>
#include <vector>
#include <cstring>
#include <atomic>
#include <poll.h>
#include <functional>
#include <sys/epoll.h>

static constexpr int defaultNumOfThreads = 1;

struct ConnectionState
{
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;

    std::vector<uint8_t> incoming_buffer;
    std::vector<uint8_t> outgoing_buffer;

    ConnectionState(int fd_) : fd{fd_} {};
};

class NetCommServer
{
public:
    NetCommServer(int port = defaultPort,
                  int numOfThreads = defaultNumOfThreads,
                  int oneSocketReadSize = defaultOneSocketReadSize,
                  int oneSocketWriteSize = defaultOneSocketWriteSize);
    ~NetCommServer();

    void run();
    void set_transform_function(transformFunctionType transformer);
    void stop();

private:
    const size_t m_one_socket_read_size{defaultOneSocketReadSize};
    const size_t m_one_socket_write_size{defaultOneSocketWriteSize};
    const int m_port{defaultPort};
    const int m_num_of_threads{defaultNumOfThreads};
    std::atomic<int> m_connections_counter{0};
    bool m_read_flag{false};
    std::mutex m_run_stop_waiter;
    std::atomic<bool> m_stopped_flag{true};
    transformFunctionType m_transformer;

    void handle_accept_select(int listeningFd, std::list<std::unique_ptr<ConnectionState>> &connections);
    void handle_accept_poll(int listeningFd, std::unordered_map<int, std::unique_ptr<ConnectionState>> &fdsToConnections);
    void handle_accept_epoll(int listeningFd, std::unordered_map<int, std::unique_ptr<ConnectionState>> &fdsToConnections, int epollFd, int threadId);
    void transform(const std::vector<uint8_t> &dataIn, std::vector<uint8_t> &dataOut);

    bool try_process_request(ConnectionState &connection);
    void handle_read(ConnectionState &connection);
    void handle_write(ConnectionState &connection);

    int serverEventLoop(int threadId);
};

void threadSafePrint(const std::string &str);