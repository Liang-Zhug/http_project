#include"TcpServer.hpp"

int main(int argc, char *argv[])    //./main  port
{
    TcpServer* tcp =  TcpServer::GetInstance(atoi(argv[1]));
    tcp->InitServer();
    return 0;
}