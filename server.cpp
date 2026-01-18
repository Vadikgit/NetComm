#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <list>
#include <unordered_map>
#include <memory>

#include <thread>
#include <vector>
#include <cstring>
#include <iostream>
#include <atomic>
#include <poll.h>
#include <sys/epoll.h>

// #define SELECT_IMPL
// #define POLL_IMPL
#define EPOLL_IMPL

typedef size_t lengthSizeType;
const size_t bytesForLengthSizeTypes = sizeof(lengthSizeType);
const size_t oneReadSize = 512;
const size_t oneWriteSize = 512;
std::atomic<int> connections_counter{0};
bool read_flag = false;

struct Request
{
    lengthSizeType len;
    std::vector<uint8_t> data;
};

struct Responce
{
    lengthSizeType len;
    std::vector<uint8_t> data;
};

struct ConnectionState
{
    int fd = -1;
    bool want_read = false;
    bool want_write = false;
    bool want_close = false;
    // bool can_read;
    // bool can_write;

    std::vector<uint8_t> incoming_buffer;
    std::vector<uint8_t> outgoing_buffer;

    ConnectionState(int fd_);
};

ConnectionState::ConnectionState(int fd_) : fd{fd_} {}

void handle_accept_select(int listeningFd, std::list<std::unique_ptr<ConnectionState>> &connections)
{
    int newSock = accept(listeningFd, NULL, NULL);
    if (newSock < 0)
    {
        return;
    }

    int flags = fcntl(newSock, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(newSock, F_SETFL, flags);

    connections.push_back(std::make_unique<ConnectionState>(newSock));
    connections.back()->want_read = true;

    connections_counter.fetch_add(1);
}

void handle_accept_poll(int listeningFd, std::unordered_map<int, std::unique_ptr<ConnectionState>> &fdsToConnections)
{
    int newSock = accept(listeningFd, NULL, NULL);
    if (newSock < 0)
    {
        return;
    }

    int flags = fcntl(newSock, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(newSock, F_SETFL, flags);

    fdsToConnections[newSock] = std::make_unique<ConnectionState>(newSock);
    fdsToConnections[newSock]->want_read = true;

    connections_counter.fetch_add(1);
}

void handle_accept_epoll(int listeningFd, std::unordered_map<int, std::unique_ptr<ConnectionState>> &fdsToConnections, int epollFd)
{
    int newSock = accept(listeningFd, NULL, NULL);
    if (newSock < 0)
    {
        return;
    }

    int flags = fcntl(newSock, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(newSock, F_SETFL, flags);

    epoll_event ev;
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.fd = newSock;

    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, newSock, &ev) == -1)
    {
        perror("epoll_ctl: EPOLL_CTL_ADD");
        exit(EXIT_FAILURE);
    }

    fdsToConnections[newSock] = std::make_unique<ConnectionState>(newSock);
    fdsToConnections[newSock]->want_read = true;

    connections_counter.fetch_add(1);
}

void transform(const std::vector<uint8_t> &dataIn, std::vector<uint8_t> &dataOut)
{
    dataOut = dataIn;
}

bool try_process_request(ConnectionState &connection)
{
    if (connection.incoming_buffer.size() < bytesForLengthSizeTypes)
    {
        return false;
    }

    size_t requestBytesLength = 0;
    requestBytesLength = *(reinterpret_cast<lengthSizeType *>(&(connection.incoming_buffer[0])));

    if (bytesForLengthSizeTypes + requestBytesLength > connection.incoming_buffer.size())
    {
        return false; // appeared only part of request
    }

    Request req;
    req.len = requestBytesLength;
    req.data.assign(connection.incoming_buffer.begin() + bytesForLengthSizeTypes, connection.incoming_buffer.begin() + bytesForLengthSizeTypes + requestBytesLength);

    Responce res;

    res.len = req.len;

    transform(req.data, res.data);

    connection.outgoing_buffer.assign(res.len + bytesForLengthSizeTypes, 0);

    *(reinterpret_cast<lengthSizeType *>(&(connection.outgoing_buffer[0]))) = res.len;
    memcpy(&(connection.outgoing_buffer[bytesForLengthSizeTypes]), &(res.data[0]), res.len);

    connection.incoming_buffer.erase(connection.incoming_buffer.begin(), connection.incoming_buffer.begin() + bytesForLengthSizeTypes + requestBytesLength);

    return true;
}

void handle_read(ConnectionState &connection)
{
    char buf[oneReadSize];
    int bytes_read = recv(connection.fd, buf, oneReadSize, MSG_NOSIGNAL);

    if (bytes_read <= 0)
    {
        connection.want_close = true;
        return;
    }

    read_flag = true;
    std::cout << "handle_read got " << bytes_read << " bytes" << std::endl;

    connection.incoming_buffer.insert(connection.incoming_buffer.end(), buf, buf + bytes_read);

    try_process_request(connection);

    if (connection.outgoing_buffer.size() > 0)
    {
        connection.want_write = true;
        connection.want_read = false;
    }
}

void handle_write(ConnectionState &connection)
{
    int bytes_write = send(connection.fd, &(connection.outgoing_buffer[0]), std::min(oneWriteSize, connection.outgoing_buffer.size()), MSG_NOSIGNAL);
    std::cout << "bytes_write: " << bytes_write << std::endl;
    if (bytes_write <= 0)
    {
        connection.want_close = true;
        return;
    }

    connection.outgoing_buffer.erase(connection.outgoing_buffer.begin(), connection.outgoing_buffer.begin() + bytes_write);

    if (connection.outgoing_buffer.size() == 0)
    {
        connection.want_write = false;
        connection.want_read = true;
    }
}

int main()
{
    int listener;
    struct sockaddr_in addr;

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0)
    {
        perror("socket");
        exit(1);
    }

    fcntl(listener, F_SETFL, O_NONBLOCK);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(3425);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(2);
    }

    listen(listener, SOMAXCONN);

    std::string impl{};

#ifdef SELECT_IMPL
    impl = "SELECT";
    std::list<std::unique_ptr<ConnectionState>> connections;
#endif

#ifdef POLL_IMPL
    impl = "POLL";
    std::unordered_map<int, std::unique_ptr<ConnectionState>> fdsToConnections; // fd -> ConnectionState ptr
#endif

#ifdef EPOLL_IMPL
    impl = "EPOLL";
    std::unordered_map<int, std::unique_ptr<ConnectionState>> fdsToConnections; // fd -> ConnectionState ptr

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        perror("epoll_create1");
        exit(1);
    }
    epoll_event ev;

    size_t max_events = 100'000;
    epoll_event events[max_events];

    ev.events = EPOLLIN;
    ev.data.fd = listener;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener, &ev) == -1)
    {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }
#endif

    std::cout << "impl: " << impl << std::endl;

    uint64_t poll_args_filling_dur_mcs{};
    uint64_t poll_call_dur_mcs{};
    uint64_t poll_res_process_dur_mcs{};
    uint64_t max_poll_res_process_dur_mcs{};

    while (1)
    {
        int milliseconds_timeout = 1000;

#ifdef SELECT_IMPL
        timeval timeout;
        timeout.tv_sec = milliseconds_timeout / 1000;
        timeout.tv_usec = 0;

        fd_set readset = {};
        FD_ZERO(&readset);
        FD_SET(listener, &readset);

        fd_set writeset = {};
        FD_ZERO(&writeset);

        fd_set errorset = {};
        FD_ZERO(&errorset);

        int mx = listener;

        for (auto connection = connections.begin(); connection != connections.end(); connection++)
        {
            mx = ((*connection)->fd > mx) ? (*connection)->fd : mx;

            if (connection->get()->want_read)
            {
                FD_SET((*connection)->fd, &readset);
            }
            if (connection->get()->want_write)
            {
                FD_SET((*connection)->fd, &writeset);
            }

            FD_SET((*connection)->fd, &errorset);
        }

        int selectRes = select(mx + 1, &readset, &writeset, &errorset, &timeout);

        // std::cout << "selectRes: " << selectRes << std::endl;
        if (selectRes <= 0)
        {
            continue;
        }

        if (FD_ISSET(listener, &readset))
        {
            handle_accept_select(listener, connections);
        }

        for (auto connection = connections.begin(); connection != connections.end();)
        {
            if (FD_ISSET(connection->get()->fd, &errorset) || connection->get()->want_close)
            {
                close(connection->get()->fd);
                connection = connections.erase(connection);

                connections_counter.fetch_sub(1);
                continue;
            }

            if (FD_ISSET(connection->get()->fd, &readset))
            {
                handle_read(*(connection->get()));
            }

            if (FD_ISSET(connection->get()->fd, &writeset))
            {
                handle_write(*(connection->get()));
            }

            ++connection;
        }
#endif

#ifdef POLL_IMPL
        std::vector<pollfd> poll_args;

        pollfd pfd = {listener, POLLIN, 0};
        poll_args.push_back(pfd);

        auto start = std::chrono::system_clock::now();

        for (auto connection = fdsToConnections.begin(); connection != fdsToConnections.end(); connection++)
        {
            pollfd pfd = {connection->second.get()->fd, POLLERR, 0};

            if (connection->second.get()->want_read)
            {
                pfd.events |= POLLIN;
            }
            if (connection->second.get()->want_write)
            {
                pfd.events |= POLLOUT;
            }

            poll_args.push_back(pfd);
        }

        auto end = std::chrono::system_clock::now();

        // auto temp_poll_args_filling_dur_mcs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        poll_args_filling_dur_mcs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // std::cout << "BEFORE connections_counter: " << connections_counter << std::endl;
        //  std::cout << "poll_args.size(): " << poll_args.size() << std::endl;

        start = std::chrono::system_clock::now();
        int pollRes = poll(poll_args.data(), (nfds_t)poll_args.size(), milliseconds_timeout);
        end = std::chrono::system_clock::now();
        // auto temp_poll_call_dur_mcs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        poll_call_dur_mcs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        if (pollRes < 0 && errno == EINTR || pollRes == 0)
        {
            continue; // not an error
        }

        // poll_args_filling_dur_mcs += temp_poll_args_filling_dur_mcs;
        // poll_call_dur_mcs += temp_poll_call_dur_mcs;

        if (pollRes < 0)
        {
            return -1;
        }

        if (poll_args[0].revents)
        {
            handle_accept_poll(listener, fdsToConnections);
        }

        start = std::chrono::system_clock::now();
        for (size_t i = 1; i < poll_args.size(); ++i)
        {

            uint32_t ready = poll_args[i].revents;

            auto &connection = *fdsToConnections[poll_args[i].fd];

            if (ready & POLLERR)
            {
                close(connection.fd);
                fdsToConnections.erase(poll_args[i].fd);

                connections_counter.fetch_sub(1);
                continue;
            }
            auto _start = std::chrono::system_clock::now();
            if (ready & POLLIN)
            {
                handle_read(connection);
            }
            if (ready & POLLOUT)
            {
                handle_write(connection);
            }
            if (connection.want_close)
            {
                close(connection.fd);
                fdsToConnections.erase(poll_args[i].fd);

                connections_counter.fetch_sub(1);
            }
            auto _end = std::chrono::system_clock::now();

            auto temp = std::chrono::duration_cast<std::chrono::microseconds>(_end - _start).count();
            if (temp > max_poll_res_process_dur_mcs)
            {
                max_poll_res_process_dur_mcs = temp;
            }
        }
        end = std::chrono::system_clock::now();
        // poll_res_process_dur_mcs += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        poll_res_process_dur_mcs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // std::cout << "AFTER connections_counter: " << connections_counter << std::endl;

        if (read_flag)
        {
            read_flag = false;
            std::cout << "poll_args_filling_dur_mcs: " << poll_args_filling_dur_mcs << std::endl;
            std::cout << "poll_call_dur_mcs: " << poll_call_dur_mcs << std::endl;
            std::cout << "poll_res_process_dur_mcs: " << poll_res_process_dur_mcs << std::endl;
            std::cout << "max_poll_res_process_dur_mcs: " << max_poll_res_process_dur_mcs << std::endl;
        }
#endif

#ifdef EPOLL_IMPL
        auto start = std::chrono::system_clock::now();
        int epoll_res = epoll_wait(epoll_fd, events, max_events, milliseconds_timeout);
        auto end = std::chrono::system_clock::now();
        // auto temp_poll_call_dur_mcs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        poll_call_dur_mcs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // poll_args_filling_dur_mcs += temp_poll_args_filling_dur_mcs;
        // poll_call_dur_mcs += temp_poll_call_dur_mcs;

        if (epoll_res == -1)
        {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        start = std::chrono::system_clock::now();
        for (int i = 0; i < epoll_res; ++i)
        {
            if (events[i].events & EPOLLERR)
            {
                if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL) == -1)
                    perror("epoll_ctl: EPOLL_CTL_DEL");

                close(events[i].data.fd);

                if (events[i].data.fd == listener)
                    return -1;

                fdsToConnections.erase(events[i].data.fd);

                connections_counter.fetch_sub(1);
                continue;
            }
            if (events[i].data.fd == listener)
            {
                handle_accept_epoll(listener, fdsToConnections, epoll_fd);
                continue;
            }

            auto &connection = *fdsToConnections[events[i].data.fd];
            auto _start = std::chrono::system_clock::now();

            if (events[i].events & EPOLLIN)
            {
                handle_read(connection);
            }
            if (events[i].events & EPOLLOUT)
            {
                handle_write(connection);
            }
            if (connection.want_close)
            {
                if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL) == -1)
                    perror("epoll_ctl: EPOLL_CTL_DEL");

                close(events[i].data.fd);

                if (events[i].data.fd == listener)
                    return -1;

                fdsToConnections.erase(events[i].data.fd);

                connections_counter.fetch_sub(1);
                continue;
            }
            auto _end = std::chrono::system_clock::now();

            auto temp = std::chrono::duration_cast<std::chrono::microseconds>(_end - _start).count();
            if (temp > max_poll_res_process_dur_mcs)
            {
                max_poll_res_process_dur_mcs = temp;
            }

            // update flags
            if (events[i].data.fd != listener)
            {
                if (connection.want_read)
                {
                    events[i].events = EPOLLIN | EPOLLRDHUP;
                }
                if (connection.want_write)
                {
                    events[i].events = EPOLLOUT | EPOLLRDHUP;
                }

                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, events[i].data.fd, &events[i]);
            }
        }
        end = std::chrono::system_clock::now();
        // poll_res_process_dur_mcs += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        poll_res_process_dur_mcs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        // std::cout << "AFTER connections_counter: " << connections_counter << std::endl;

        if (read_flag)
        {
            read_flag = false;
            std::cout << "poll_args_filling_dur_mcs: " << poll_args_filling_dur_mcs << std::endl;
            std::cout << "poll_call_dur_mcs: " << poll_call_dur_mcs << std::endl;
            std::cout << "poll_res_process_dur_mcs: " << poll_res_process_dur_mcs << std::endl;
            std::cout << "max_poll_res_process_dur_mcs: " << max_poll_res_process_dur_mcs << std::endl;
        }
#endif
    }

    return 0;
}