#include "client.h"
#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <thread>

std::string_view ipAddrParamString = "address";
std::string_view portParamString = "port";
std::string_view connectionsPerThreadParamString = "connections";

constexpr std::string_view defaultServerIpAddr = "127.0.0.1";
constexpr int defaultServerPort = defaultPort;
constexpr int defaultNumOfConnections = 2;

void processCmdLineArgs(int argc, char **argv, std::string &ipAddr, int &port, int &connections_num)
{
    ipAddr = defaultServerIpAddr;
    port = defaultServerPort;
    connections_num = defaultNumOfConnections;

    for (size_t i = 1; i < argc;)
    {
        if (std::string_view(argv[i]) == ipAddrParamString)
            ipAddr = std::string(argv[i]);
        else if (std::string_view(argv[i]) == portParamString)
            port = atoi(argv[++i]);
        else if (std::string_view(argv[i]) == connectionsPerThreadParamString)
            connections_num = atoi(argv[++i]);

        ++i;
    }
}

int main(int argc, char **argv)
{
    std::string ipAddr;
    int port;
    int numOfConnections;

    processCmdLineArgs(argc, argv, ipAddr, port, numOfConnections);

    auto start = std::chrono::system_clock::now();

    std::vector<std::unique_ptr<NetCommClient>> clients;
    for (size_t i = 0; i < numOfConnections; i++)
    {
        clients.emplace_back(std::make_unique<NetCommClient>(ipAddr, port));
    }

    std::string msg{};
    msg.assign(1024, 0);

    for (int i = 0; i < 1024; i++)
    {
        msg[i] = '0' + (i % 10);
    }

    std::string respStr;

    // std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    clients[numOfConnections / 2]->send_bytes(msg);

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    auto end = std::chrono::system_clock::now();

    std::cout << "Total duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;

    return 0;
}