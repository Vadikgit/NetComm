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

// #define SELECT_IMPL
#define POLL_IMPL

typedef size_t lengthSizeType;
const size_t bytesForLengthSizeTypes = sizeof(lengthSizeType);
const size_t oneReadSize = 512;
const size_t oneWriteSize = 512;
std::atomic<int> connections_counter{0};

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

    std::cout << "impl: " << impl << std::endl;

    while (1)
    {
        std::cout << "connections_counter: " << connections_counter << std::endl;

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

        for (auto connection = fdsToConnections.begin(); connection != fdsToConnections.end(); connection++)
        {
            pollfd pfd = {connection->second.get()->fd, POLLERR, 0};

            poll_args.push_back(pfd);

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

        int pollRes = poll(poll_args.data(), (nfds_t)poll_args.size(), milliseconds_timeout);
        if (pollRes < 0 && errno == EINTR || pollRes == 0)
        {
            continue; // not an error
        }
        if (pollRes < 0)
        {
            return -1;
        }

        if (poll_args[0].revents)
        {
            handle_accept_poll(listener, fdsToConnections);
        }

        for (size_t i = 1; i < poll_args.size(); ++i)
        {
            uint32_t ready = poll_args[i].revents;

            auto &connection = *fdsToConnections[poll_args[i].fd];

            if (ready & POLLIN)
            {
                handle_read(connection);
            }
            if (ready & POLLOUT)
            {
                handle_write(connection);
            }
            if (ready & POLLERR || connection.want_close)
            {
                close(connection.fd);
                fdsToConnections.erase(poll_args[i].fd);

                connections_counter.fetch_sub(1);
                continue;
            }
        }
#endif
    }

    return 0;
}