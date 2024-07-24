#pragma once
#include"Common.h"

const static int WAIT_LENGTH = 5;   //全连接队列的长度为WAIT_LENGTH + 1
class TcpServer
{
private:
    int _port;      //服务器程序绑定端口
    int _listenSock;    //监听套接字
private:
    static TcpServer *_tsvr;    //单例模式
    TcpServer(int port) : _port(port),_listenSock(-1){}
    TcpServer(const TcpServer &cp) = delete;
    TcpServer &operator = (const TcpServer &cp) = delete;
public:
    static TcpServer *GetInstance(int port)
    {
        if(_tsvr == nullptr)
        {
            static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
            pthread_mutex_lock(&mtx);
            if(_tsvr == nullptr)
            {
                _tsvr = new TcpServer(port);
            }
            pthread_mutex_unlock(&mtx);
        }
        return _tsvr;
    }

    void InitServer()
    {
        Socket();   //创建监听套接字
        Bind();     //绑定IP与port
        Listen();   //
    }

    void Socket()
    {                     //字节流，TCP协议
        _listenSock = socket(AF_INET,SOCK_STREAM,0);
        if(_listenSock < 0)// 创建监听套接字失败
        {
            LOG(FATAL,"create listen sock fail");
            exit(1);
        }
        int opt = 1;//设置地址复用，防止服务器崩掉后进入TIME_WAIT,短时间连不上端口
        setsockopt(_listenSock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
        LOG(INFO, "create listen sock success");
    }
    void Bind()
    {
        sockaddr_in local;  
        memset(&local,0,sizeof(local));
        local.sin_family = AF_INET;     //IPV4协议
        local.sin_port = htons(_port);  //主机转网络字节序
        local.sin_addr.s_addr = INADDR_ANY; //能接受来自任意网卡的地址
        if(bind(_listenSock,(sockaddr*)&local,sizeof(local))<0)
        {
            LOG(FATAL,"bind fail");
            exit(2);

        }
        LOG(INFO,"bind success");
    }
    void Listen()
    {
        if(listen(_listenSock,WAIT_LENGTH) < 0) //监听，未决连接的数量为WAIT_LENGTH
        {
            LOG(FATAL, "listen fail");
            exit(3);
        }
        LOG(INFO, "listen success");
    }
    int ListenSocket()
    {
        return _listenSock;
    }

    ~TcpServer()
    {
        if(_listenSock > 0)
        {
            close(_listenSock);
        }
    }
};
TcpServer *TcpServer::_tsvr = nullptr;  //单例模式对象