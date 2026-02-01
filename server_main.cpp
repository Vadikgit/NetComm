#include "server.h"
#include <iostream>

std::string_view portParamString = "port";
std::string_view numOfThreadsParamString = "threads";

void processCmdLineArgs(int argc, char **argv, int &threads_num, int &port)
{
    threads_num = -1;
    port = -1;

    for (size_t i = 1; i < argc;)
    {
        if (std::string_view(argv[i]) == portParamString)
        {
            port = atoi(argv[++i]);
        }
        else if (std::string_view(argv[i]) == numOfThreadsParamString)
        {
            threads_num = atoi(argv[++i]);
        }
        ++i;
    }

    if (threads_num == -1)
    {
        threads_num = defaultNumOfThreads;
    }
    if (port == -1)
    {
        port = defaultPort;
    }
}

int main(int argc, char **argv)
{
    int threads_num{};
    int port{};

    processCmdLineArgs(argc, argv, threads_num, port);

    NetCommServer server{port, threads_num};

    server.run();

    return 0;
}