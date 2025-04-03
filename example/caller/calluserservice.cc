#include <iostream>
#include "mprpcapplication.h"
#include "user.pb.h"
#include "mprpcchannel.h"

int main(int argc, char **argv) {
    //  整个程序启动以后，想使用MPRPC框架来享用rpc服务调用，一定要先调用框架的初始化函数(只初始化1次)
    MprpcApplication::Init(argc, argv);
    
    //  演示调用远程发布的RPC方法Login
    fixbug::UserServiceRpc_Stub stub(new MprpcChannel());
    //  rpc方法的请求参数
    fixbug::LoginRequest request;
    request.set_name("zhang san");
    request.set_pwd("1231231");
    //  rpc方法的响应
    fixbug::LoginResponse response;
    //  发起rpc方法的调用，同步的rpc调用过程  
    stub.Login(nullptr, &request, &response, nullptr);
    //  一次rpc调用完成，读调用的结果
    if(response.result().errcode() == 0){
        std::cout << "rpc login response:" << response.sucess() << std::endl;
    }
    else{
        std::cout << "rpc login response:" << response.result().errmsg() << std::endl;
    }

    //  演示调用远程发布的RPC方法Register
    fixbug::RegisterRequest req;
    req.set_id(2000);
    req.set_name("MPRPC");
    req.set_pwd("666666");
    fixbug::RegisterResponse rep;
    
    //  发起rpc方法的调用，同步的rpc调用过程    
    stub.Register(nullptr, &req, &rep, nullptr);

    //  一次rpc调用完成，读调用的结果
    if(rep.result().errcode() == 0){
        std::cout << "rpc login response:" << rep.sucess() << std::endl;
    }
    else{
        std::cout << "rpc login response:" << rep.result().errmsg() << std::endl;
    }

    return 0;
}