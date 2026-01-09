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

    size_t already_sent = 0;

    while (already_sent != data.length())
    {
        int sent = send(sockfd, &(data[0 + already_sent]), std::min(oneWriteSize, data.length() - already_sent), 0);

        if (sent <= 0)
        {
            return;
        }

        already_sent += sent;
    }

    data.erase(data.begin(), data.begin() + sizeof(size_t));
}

void getBytes(int sockfd, std::string &data)
{
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

    memcpy(&(data[0]), buf + sizeof(numberOfBytesToGet), received_total - sizeof(numberOfBytesToGet));

    while (received_total < sizeof(numberOfBytesToGet) + numberOfBytesToGet)
    {
        recieved = recv(sockfd, buf, oneReadSize, 0);

        if (recieved <= 0)
        {
            return;
        }

        memcpy(&(data[received_total - sizeof(numberOfBytesToGet)]), buf, recieved);

        received_total += recieved;
    }
}

void work(int myId, int numOfRequestsPerThread, int numOfBytesForOneRequest, int delayBetveenRequestsInMcs)
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
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        exit(2);
    }

    std::string msg{};
    msg.assign(numOfBytesForOneRequest, 0);

    for (int i = 0; i < numOfBytesForOneRequest; i++)
    {
        msg[i] = '0' + (i % 10);
    }

    std::string respStr;

    for (size_t i = 0; i < numOfRequestsPerThread; i++)
    {
        auto start = std::chrono::system_clock::now();

        sendBytes(sock, msg);

        auto end = std::chrono::system_clock::now();

        // std::cout << "CLIENT [" << myId << "] request (" << i << "), Sending completed for " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " mcs" << std::endl;

        start = std::chrono::system_clock::now();

        getBytes(sock, respStr);

        end = std::chrono::system_clock::now();

        // std::cout << "CLIENT [" << myId << "] request (" << i << "), Receiving completed for " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " mcs" << std::endl;

        std::this_thread::sleep_for(std::chrono::microseconds(delayBetveenRequestsInMcs));
    }
    close(sock);
}

int main()
{
    int numOfThreads = 4;
    int numOfRequestsPerThread = 10;
    int numOfBytesForOneRequest = 1024;
    int delayBetveenRequestsInMcs = 0;

    std::cout << std::endl
              << std::endl
              << std::thread::hardware_concurrency() << std::endl;

    std::vector<std::thread> clients;

    auto start = std::chrono::system_clock::now();

    for (size_t i = 0; i < numOfThreads; i++)
    {
        clients.emplace_back(work, i, numOfRequestsPerThread, numOfBytesForOneRequest, delayBetveenRequestsInMcs);
    }

    for (auto &i : clients)
        i.join();

    auto end = std::chrono::system_clock::now();

    std::cout << "Total duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;

    return 0;
}