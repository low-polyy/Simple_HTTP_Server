#include "http_tcpServer.h"

int main()
{
    TCPServer myServer = TCPServer("0.0.0.0",8080);
    myServer.startListen();
    return 0;
}