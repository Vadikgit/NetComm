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

const size_t oneReadSize = 1024;
const size_t oneWriteSize = 1024;

void sendBytes(int sockfd, std::string &data)
{
    data.insert(data.begin(), sizeof(size_t), 0);
    *(reinterpret_cast<size_t *>(&(data[0]))) = data.size() - sizeof(size_t);

    std::cout << "I want to send " << data.length() << " bytes" << std::endl;

    size_t already_sent = 0;

    auto start = std::chrono::system_clock::now();

    while (already_sent != data.length())
    {
        int sent = send(sockfd, &(data[0 + already_sent]), std::min(oneWriteSize, data.length() - already_sent), 0);

        if (sent <= 0)
        {
            return;
        }
        std::cout << "  >> sent " << sent << " bytes" << std::endl;
        already_sent += sent;
    }

    auto end = std::chrono::system_clock::now();

    data.erase(data.begin(), data.begin() + sizeof(size_t));

    std::cout << "Completed for " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " mcs" << std::endl;
}

void getBytes(int sockfd, std::string &data)
{
    auto start = std::chrono::system_clock::now();

    char buf[oneReadSize];

    int recieved = 0;
    size_t received_total = 0;

    while (received_total < sizeof(size_t))
    {
        recieved = recv(sockfd, buf + received_total, oneReadSize - received_total, 0);

        if (recieved <= 0)
        {
            return;
        }

        received_total += recieved;
    }

    size_t numberOfBytesToGet = 0;
    numberOfBytesToGet = *(reinterpret_cast<size_t *>(buf));

    data.assign(numberOfBytesToGet, 0);

    std::cout << "Server want to send " << numberOfBytesToGet << " bytes + sizeof(size_t)" << std::endl;

    memcpy(&(data[0]), buf + sizeof(numberOfBytesToGet), received_total - sizeof(numberOfBytesToGet));

    std::cout << "  >> received " << received_total << " bytes" << std::endl;

    while (received_total < sizeof(numberOfBytesToGet) + numberOfBytesToGet)
    {
        recieved = recv(sockfd, buf, oneReadSize, 0);

        if (recieved <= 0)
        {
            return;
        }

        memcpy(&(data[received_total - sizeof(numberOfBytesToGet)]), buf, recieved);

        std::cout << "  >> received " << recieved << " bytes" << std::endl;

        received_total += recieved;
    }

    auto end = std::chrono::system_clock::now();

    std::cout << "Completed for " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " mcs" << std::endl;
}

int main()
{
    int sock;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("socket");
        exit(1);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(3425);
    // addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        exit(2);
    }

    std::string msg{};

    for (int i = 0; i < 32 * 5; i++)
    {
        std::string tempstr{"Data string consist of 32 bytes."};
        msg.append(tempstr);
    }

    sendBytes(sock, msg);

    std::string respStr;
    getBytes(sock, respStr);

    std::cout << "respStr: " << respStr << std::endl;

    close(sock);

    return 0;
}