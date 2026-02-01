#include "client.h"
#include <iostream>

int main(int argc, char **argv)
{
    NetCommClient client{};

    std::string msg{};

    for (int i = 0; i < 32 * 5; i++)
    {
        std::string tempstr{"Data string consist of 32 bytes."};
        msg.append(tempstr);
    }

    client.send_bytes(msg);

    std::string respStr;
    client.get_bytes(respStr);

    std::cout << "respStr: " << respStr << std::endl;

    return 0;
}