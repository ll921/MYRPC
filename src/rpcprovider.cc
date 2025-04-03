#include "rpcprovider.h"
#include "mprpcapplication.h"
#include "rpcheader.pb.h"
#include "logger.h"
#include "zookeeperutil.h"


//  可以发布RPC方法的接口
void RpcProvider::NotifyService(google::protobuf::Service *service){
    ServiceInfo service_info;

    //  获取了服务对象的描述信息 pserviceDesc获取的是.proto中的元信息
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor();
    //  获取服务的名字  service_name:UserServiceRpc
    std::string service_name = pserviceDesc->name();
    //  获取服务对象service的方法和数量
    int methodCnt = pserviceDesc->method_count();
    LOG_INFO("service_name:%s", service_name.c_str());

    for (int i = 0; i < methodCnt; ++i){
        //  获取服务对象指定下标的服务方法的描述(抽象描述)
        //  pmethodDesc是某个RPC方法的元信息
        const google::protobuf::MethodDescriptor* pmethodDesc =  pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});
        
        //  std::cout << "method_name:" << method_name << std::endl;
        LOG_INFO("method_name:%s", method_name.c_str());
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}

//  启动RPC服务结点，开始提供RPC远程网络调用服务
void RpcProvider::Run(){
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    //  创建TcpServer对象
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");

    //  绑定连接回调和消息读写回调方法
    server.setConnectionCallback(std::bind(&RpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&RpcProvider::OnMessage, this, std::placeholders::_1,
             std::placeholders::_2, std::placeholders::_3));
    
    //设置muduo库的线程数量
    server.setThreadNum(4);
    //服务注册
    ZkClient zkcli;
    zkcli.Start();
    //  service_name 为永久性结点   method_name为临时性结点
    for(auto &sp : m_serviceMap){
        //  service_name
        std::string service_path = "/" + sp.first;
        zkcli.Create(service_path.c_str(), nullptr, 0);
        for(auto &mp : sp.second.m_methodMap){
            //  /service_name/method_name   /UserServiceRpc/Login  存储当前这个rpc服务结点主机的ip和port
            std::string method_path = service_path + "/" + mp.first;
            zkcli.Create(method_path.c_str(), nullptr, 0);
            std::string node_path = method_path + "/" + serverName;
            char node_path_data[128] = {0};
            sprintf(node_path_data, "%s:%d", ip.c_str(), port);
            zkcli.Create(node_path.c_str(), node_path_data, strlen(node_path_data), ZOO_EPHEMERAL);
        }
    }

    LOG_INFO("RpcProvider start service at ip::%s port:%d", ip.c_str(), port);

    //启动网络服务
    server.start();
    m_eventLoop.loop();
}

//  新的socket连接回调
void RpcProvider::OnConnection(const muduo::net::TcpConnectionPtr& conn){
    if(!conn->connected()){
        //  和rpc  client的连接断开了
        conn->shutdown();
    }
}

//  已建立连接用户的读写事件回调 ，如果远程有一个rpc服务的调用请求，那么OnMessage方法就会响应
void RpcProvider::OnMessage(const muduo::net::TcpConnectionPtr& conn, 
                            muduo::net::Buffer *buffer,
                             muduo::Timestamp){
    //  网络上接收远程RPC调用请求的字符流  
    std::string recv_buf = buffer->retrieveAllAsString();

    //  从字符流中读取前四个字节的内容
    //  数据格式 header_size(4字节)+header_str(header_size字节)+(args_str)
    uint32_t header_size = 0;
    recv_buf.copy((char *)&header_size, 4, 0);

    //  根据header_size读取数据头的原始字符流,反序列化数据,得到rpc请求的详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if(rpcHeader.ParseFromString(rpc_header_str)){
        //  数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else{
        //  数据头反序列化失败
        LOG_ERR("rec_header_str:%sparse error!", rpc_header_str.c_str());
        return;
    }

    //  获取rpc方法参数的字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);


    //  获取serivce对象和method对象
    auto it = m_serviceMap.find(service_name);
    if(it == m_serviceMap.end()){
        LOG_ERR("%sis not exist", service_name.c_str());
        return;
    }

    auto mit = it->second.m_methodMap.find(method_name);
    if(mit == it->second.m_methodMap.end()){
        LOG_ERR("%s:%sis not exist", service_name.c_str(), method_name.c_str());
        return;
    }

    google::protobuf::Service *service = it->second.m_service;  
    const google::protobuf::MethodDescriptor *method = mit->second; 

    //  生成rpc方法调用的请求request和响应response参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if(!request->ParseFromString(args_str)){
        LOG_ERR("request parse error! content:%s", args_str.c_str());
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    //  给下面的method的方法的调用，绑定一个Closure的回调函数
    google::protobuf::Closure *done = google::protobuf::NewCallback (this, 
                                                                    &RpcProvider::SendRpcResponse, 
                                                                    conn, response);

    //  在框架上根据远端的RPC请求，调用当前RPC结点的方法
    service->CallMethod(method, nullptr, request, response, done);
}

//  Closure的回调操作，用于序列化RPC的响应和网络发送
void RpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message *response){
    std::string response_str;
    //  response序列化
    if(response->SerializeToString(&response_str)){
        //  序列化成功后，通过网络吧RPC方法执行结果发送回RPC的调用方
        conn->send(response_str);
    }
    else{
        LOG_ERR("serialize response_str error!");
    }
    conn->shutdown();  
}

void RpcProvider::setServerName(std::string name)
{
    serverName = name;
}