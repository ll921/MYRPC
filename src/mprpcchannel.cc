#include "mprpcchannel.h"
#include "rpcheader.pb.h"
#include "mprpcapplication.h"
#include "mprpccontroller.h"
#include "zookeeperutil.h"
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <error.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>


/*
header_size + service_name + method_name + args_name + args
*/
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller, const google::protobuf::Message* request,
                          google::protobuf::Message* response, google::protobuf::Closure* done){

    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name();  //  service_name
    std::string method_name = method->name();   //  method_name

    //  获取参数的序列化字符串长度  args_size
    uint32_t args_size = 0;
    std::string args_str;
    if(request->SerializeToString(&args_str)){
        args_size = args_str.size();
    }
    else{
        controller->SetFailed("serialize request error!");
        return;
    }

    //  定义rpc的请求header
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if(rpcHeader.SerializeToString(&rpc_header_str)){
        header_size = rpc_header_str.size();
    }
    else{
        controller->SetFailed("serialize rpc header error!");
        return;
    }

    //  组织待发送的rpc请求的字符串
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char*)&header_size, 4));    //  header_size
    send_rpc_str += rpc_header_str; //  rpcheader
    send_rpc_str += args_str;   //  args

    //  使用TCP编程完成RPC方法的远程调用
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(client_fd == -1){
        //  std::cout << "create socket error! error:" << errno << std::endl;
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! error:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    //  读配置文件rpcserver的信息
    ZkClient zkcli;
    zkcli.Start();
    std::string method_path = "/" + service_name + "/" + method_name;
    //  127.0.0.1:8080      bug=>method_path写成了mathod_name
    std::string host_data = zkcli.GetNode(method_path);
    if(host_data == ""){
        controller->SetFailed(method_path + "is not exist!");
        return;
    }
    int idx = host_data.find(":");
    if(idx == -1){
        controller->SetFailed(method_path + "address is invalid!");
        return;
    }
    std::string ip = host_data.substr(0, idx);
    uint16_t port = atoi(host_data.substr(idx + 1, host_data.size() - idx).c_str());

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    //  连接RPC服务结点
    if(connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
        close(client_fd);
        char errtxt[512] = {0};
        sprintf(errtxt, "connect error! error:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    //  发送RPC请求         
    if(send(client_fd, send_rpc_str.c_str(), send_rpc_str.size(), 0) == -1){
        close(client_fd);
        char errtxt[512] = {0};
        sprintf(errtxt, "send error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    //  接收RPC请求的响应值
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if((recv_size = recv(client_fd, recv_buf, 1024, 0)) == -1){
        close(client_fd);
        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! error:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    //  反序列化RPC调用的响应数据
    if(!response->ParseFromArray(recv_buf, recv_size)){
        close(client_fd);
        char errtxt[512] = {0};
        sprintf(errtxt, "parse error! response_str%s", recv_buf);
        controller->SetFailed(errtxt);
        return;
    }
    close(client_fd);

}