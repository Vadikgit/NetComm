#include "client.h"
#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <thread>

void work(int myId, std::string ipAddr, int port,
          int numOfConnectionsPerThread,
          int numOfRequestsPerConnection,
          int numOfBytesForOneRequest,
          int delayBetveenRequestsInMcs,
          bool sendThreadConnectionsRequestsSimultaneously)
{

    if (sendThreadConnectionsRequestsSimultaneously == false)
    {
        for (size_t i = 0; i < numOfConnectionsPerThread; i++)
        {
            NetCommClient client{ipAddr, port};

            std::string msg{};
            msg.assign(numOfBytesForOneRequest, 0);

            for (int j = 0; j < numOfBytesForOneRequest; j++)
            {
                msg[j] = '0' + (j % 10);
            }

            for (size_t j = 0; j < numOfRequestsPerConnection; j++)
            {
                std::string respStr{};

                auto start = std::chrono::system_clock::now();

                client.send_bytes(msg);

                auto end = std::chrono::system_clock::now();

                // std::cout << "CLIENT [" << myId << "] request (" << i << "), Sending completed for " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " mcs" << std::endl;

                start = std::chrono::system_clock::now();

                client.get_bytes(respStr);

                end = std::chrono::system_clock::now();

                if (respStr.size() != msg.size())
                {
                    std::cerr << "req data size != resp data size (" << msg.size() << " != " << respStr.size() << ")" << std::endl;
                }

                // std::cout << "CLIENT [" << myId << "] request (" << i << "), Receiving completed for " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " mcs" << std::endl;

                std::this_thread::sleep_for(std::chrono::microseconds(delayBetveenRequestsInMcs));
            }
        }
    }
    else
    {
        std::vector<std::unique_ptr<NetCommClient>> clients;
        for (size_t i = 0; i < numOfConnectionsPerThread; i++)
        {
            clients.emplace_back(std::make_unique<NetCommClient>(ipAddr, port));
        }

        for (size_t i = 0; i < numOfRequestsPerConnection; i++)
        {
            std::string msg{};
            msg.assign(numOfBytesForOneRequest, 0);

            for (int j = 0; j < numOfBytesForOneRequest; j++)
            {
                msg[j] = '0' + (j % 10);
            }

            for (size_t j = 0; j < numOfConnectionsPerThread; j++)
            {
                std::string respStr{};

                auto start = std::chrono::system_clock::now();

                clients[j]->send_bytes(msg);

                auto end = std::chrono::system_clock::now();

                // std::cout << "CLIENT [" << myId << "] request (" << i << "), Sending completed for " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " mcs" << std::endl;

                start = std::chrono::system_clock::now();

                clients[j]->get_bytes(respStr);

                end = std::chrono::system_clock::now();

                if (respStr.size() != msg.size())
                {
                    std::cerr << "req data size != resp data size (" << msg.size() << " != " << respStr.size() << ")" << std::endl;
                }

                // std::cout << "CLIENT [" << myId << "] request (" << i << "), Receiving completed for " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " mcs" << std::endl;

                std::this_thread::sleep_for(std::chrono::microseconds(delayBetveenRequestsInMcs));
            }
        }
    }
}

std::string_view ipAddrParamString = "address";
std::string_view portParamString = "port";
std::string_view numOfThreadsParamString = "threads";
std::string_view connectionsPerThreadParamString = "connections";
std::string_view requestsPerConnectionParamString = "requests";
std::string_view sendThreadConnectionsRequestsSimultaneouslyParamString = "parallel";
std::string_view numOfBytesForOneRequestParamString = "bytes";
std::string_view delayBetveenRequestsInMcsParamString = "delay";

constexpr std::string_view defaultServerIpAddr = "127.0.0.1";
constexpr int defaultServerPort = defaultPort;
constexpr int defaultNumOfThreads = 1;
constexpr int defaultNumOfConnectionsPerThread = 1;
constexpr int defaultNumOfRequestsPerConnection = 10;
constexpr bool defaultSendThreadConnectionsRequestsSimultaneously = false;
constexpr int defaultNumOfBytesForOneRequest = 1024;
constexpr int defaultDelayBetveenRequestsInMcs = 0;

void processCmdLineArgs(int argc, char **argv,
                        std::string &ipAddr,
                        int &port,
                        int &threads_num,
                        int &connections_per_thread,
                        int &requests_per_connection,
                        bool &parallel_send,
                        int &bytes_for_one_request,
                        int &delay_betveen_requests)
{
    ipAddr = defaultServerIpAddr;
    port = defaultServerPort;
    threads_num = defaultNumOfThreads;
    connections_per_thread = defaultNumOfConnectionsPerThread;
    requests_per_connection = defaultNumOfRequestsPerConnection;
    parallel_send = defaultSendThreadConnectionsRequestsSimultaneously;
    bytes_for_one_request = defaultNumOfBytesForOneRequest;
    delay_betveen_requests = defaultDelayBetveenRequestsInMcs;

    for (size_t i = 1; i < argc;)
    {
        if (std::string_view(argv[i]) == ipAddrParamString)
            ipAddr = std::string(argv[i]);
        else if (std::string_view(argv[i]) == portParamString)
            port = atoi(argv[++i]);
        else if (std::string_view(argv[i]) == numOfThreadsParamString)
            threads_num = atoi(argv[++i]);
        else if (std::string_view(argv[i]) == connectionsPerThreadParamString)
            connections_per_thread = atoi(argv[++i]);
        else if (std::string_view(argv[i]) == requestsPerConnectionParamString)
            requests_per_connection = atoi(argv[++i]);
        else if (std::string_view(argv[i]) == sendThreadConnectionsRequestsSimultaneouslyParamString)
            parallel_send = true;
        else if (std::string_view(argv[i]) == numOfBytesForOneRequestParamString)
            bytes_for_one_request = atoi(argv[++i]);
        else if (std::string_view(argv[i]) == delayBetveenRequestsInMcsParamString)
            delay_betveen_requests = atoi(argv[++i]);

        ++i;
    }
}

int main(int argc, char **argv)
{
    std::string ipAddr;
    int port;
    int numOfThreads;
    int numOfConnectionsPerThread;
    int numOfRequestsPerConnection;
    bool sendThreadConnectionsRequestsSimultaneously;
    int numOfBytesForOneRequest;
    int delayBetveenRequestsInMcs;

    processCmdLineArgs(argc, argv,
                       ipAddr, port, numOfThreads,
                       numOfConnectionsPerThread,
                       numOfRequestsPerConnection,
                       sendThreadConnectionsRequestsSimultaneously,
                       numOfBytesForOneRequest,
                       delayBetveenRequestsInMcs);

    std::vector<std::thread> clients;

    auto start = std::chrono::system_clock::now();

    for (size_t i = 0; i < numOfThreads - 1; i++)
    {
        clients.emplace_back(work, i, ipAddr, port,
                             numOfConnectionsPerThread,
                             numOfRequestsPerConnection,
                             numOfBytesForOneRequest,
                             delayBetveenRequestsInMcs,
                             sendThreadConnectionsRequestsSimultaneously);
    }

    work(numOfThreads - 1, ipAddr, port,
         numOfConnectionsPerThread,
         numOfRequestsPerConnection,
         numOfBytesForOneRequest,
         delayBetveenRequestsInMcs,
         sendThreadConnectionsRequestsSimultaneously);

    for (auto &i : clients)
        i.join();

    auto end = std::chrono::system_clock::now();

    std::cout << "Total duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;

    return 0;
}