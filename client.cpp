#include "client.h"

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

NetCommClient::NetCommClient(const std::string &ipAddr, int port, int oneSocketReadSize, int oneSocketWriteSize)
    : m_one_socket_read_size{oneSocketReadSize}, m_one_socket_write_size{oneSocketWriteSize}
{
    struct sockaddr_in addr;

    m_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_sock_fd < 0)
    {
        perror("socket");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ipAddr.c_str());
    if (connect(m_sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        exit(2);
    }
}

NetCommClient::~NetCommClient()
{
    close(m_sock_fd);
}

void NetCommClient::send_bytes(const std::string &data)
{
    size_t already_sent = 0;
    lengthSizeType dataSize = data.size();
    int sent = 0;

    auto start = std::chrono::system_clock::now();

    while (already_sent != sizeof(dataSize))
    {
        sent = send(m_sock_fd, &dataSize, std::min(m_one_socket_write_size, sizeof(dataSize) - already_sent), MSG_MORE); // prevent send without data

        if (sent <= 0)
        {
            return;
        }
        already_sent += sent;
    }

    // std::cout << "I want to send " << data.length() << " bytes" << std::endl;

    while (already_sent != data.length() + sizeof(dataSize))
    {
        sent = send(m_sock_fd, &(data[0 + already_sent - sizeof(dataSize)]), std::min(m_one_socket_write_size, data.length() + sizeof(dataSize) - already_sent), 0);

        if (sent <= 0)
        {
            return;
        }
        // std::cout << "  >> sent " << sent << " bytes" << std::endl;
        already_sent += sent;
    }

    auto end = std::chrono::system_clock::now();

    // std::cout << "Completed for " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " mcs" << std::endl;
}

void NetCommClient::get_bytes(std::string &data)
{
    auto start = std::chrono::system_clock::now();

    char buf[m_one_socket_read_size];

    int recieved = 0;
    size_t received_total = 0;

    while (received_total < sizeof(lengthSizeType))
    {
        recieved = recv(m_sock_fd, buf + received_total, m_one_socket_read_size - received_total, 0);

        if (recieved <= 0)
        {
            return;
        }

        received_total += recieved;
    }

    size_t numberOfBytesToGet = 0;
    numberOfBytesToGet = *(reinterpret_cast<lengthSizeType *>(buf));

    data.assign(numberOfBytesToGet, 0);

    // std::cout << "Server want to send " << numberOfBytesToGet << " bytes + sizeof(size_t)" << std::endl;

    memcpy(&(data[0]), buf + sizeof(numberOfBytesToGet), received_total - sizeof(numberOfBytesToGet));
    // std::cout << "  >> received " << received_total << " bytes" << std::endl;

    while (received_total < sizeof(numberOfBytesToGet) + numberOfBytesToGet)
    {
        recieved = recv(m_sock_fd, buf, m_one_socket_read_size, 0);

        if (recieved <= 0)
        {
            return;
        }

        memcpy(&(data[received_total - sizeof(numberOfBytesToGet)]), buf, recieved);

        // std::cout << "  >> received " << recieved << " bytes" << std::endl;

        received_total += recieved;
    }

    auto end = std::chrono::system_clock::now();

    // std::cout << "Completed for " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " mcs" << std::endl;
}
