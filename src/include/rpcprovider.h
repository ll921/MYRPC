#pragma once 

#include "google/protobuf/service.h"
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <string>
#include <functional>
#include <google/protobuf/descriptor.h>
#include <unordered_map>

//  框架提供的专门发布rpc服务的网络对象类
class RpcProvider{
public:
    //  可以发布RPC方法的接口
    void NotifyService(google::protobuf::Service *service);

    //  设置主机名  
    void setServerName(std::string name);

    //  启动RPC服务结点，开始提供RPC远程网络调用服务
    void Run();

private:
    //  组合了EventLoop
    muduo::net::EventLoop m_eventLoop;

    //  service服务类型信息
    struct ServiceInfo{
        google::protobuf::Service *m_service;    //  保存服务对象
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> m_methodMap;   //  保存服务方法
    };
    //  存储注册成功的服务对象和其服务方法的所有信息
    std::unordered_map<std::string, ServiceInfo> m_serviceMap;


    std::string serverName;

    //  新的socket连接回调
    void OnConnection(const muduo::net::TcpConnectionPtr&);

    //  已建立连接用户的读写事件回调
    void OnMessage(const muduo::net::TcpConnectionPtr&, muduo::net::Buffer*, muduo::Timestamp);

    //  Closure的回调操作，用于序列化RPC的响应和网络操作
    void SendRpcResponse(const muduo::net::TcpConnectionPtr&, google::protobuf::Message*);

};