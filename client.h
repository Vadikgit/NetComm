#pragma once

#include "common.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

#include <arpa/inet.h>
#include <cstring>

class NetCommClient
{
public:
    NetCommClient(const std::string &ipAddr = "127.0.0.1",
                  int port = defaultPort,
                  int oneSocketReadSize = defaultOneSocketReadSize,
                  int oneSocketWriteSize = defaultOneSocketWriteSize);
    ~NetCommClient();

    void send_bytes(const std::string &data);
    void send_bytes(const std::vector<uint8_t> &data);
    void get_bytes(std::string &data);
    void get_bytes(std::vector<uint8_t> &data);

private:
    void send_bytes_impl(const uint8_t *data, lengthSizeType len);

    template <typename T>
    void get_bytes_impl(T &data);

    const size_t m_one_socket_read_size{defaultOneSocketReadSize};
    const size_t m_one_socket_write_size{defaultOneSocketWriteSize};
    int m_sock_fd{-1};
};