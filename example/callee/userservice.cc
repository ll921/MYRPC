#include <iostream>
#include <string>
#include "user.pb.h"
#include "mprpcapplication.h"
#include "rpcprovider.h"

class UserService : public fixbug::UserServiceRpc{  
public:
    bool Login(std::string name, std::string pwd){
        std::cout << "doing local service: Login" << std::endl;
        std::cout << "name:" << name << "pwd:" << pwd << std::endl;
        return true;    
    }

    bool Register(uint32_t id, std::string name, std::string pwd){
        std::cout << "doing local service: Register" << std::endl;
        std::cout <<  "id:" << id << "name:" << name << "pwd:" << pwd << std::endl;
        return true;
    }

    void Login(::google::protobuf::RpcController* controller,
                       const ::fixbug::LoginRequest* request,
                       ::fixbug::LoginResponse* response,
                       ::google::protobuf::Closure* done){
        std::string name = request->name();
        std::string pwd = request->pwd();

        //  做本地业务
        bool login_result = Login(name, pwd);

        //  把响应写入   包括错误码   错误消息  返回值 
        fixbug::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_sucess(login_result);

        //   执行回调函数   执行响应对象数据的序列化和网络发送(都是由框架来完成的)
        done->Run();
    }

    void Register(::google::protobuf::RpcController* controller,
                       const ::fixbug::RegisterRequest* request,
                       ::fixbug::RegisterResponse* response,
                       ::google::protobuf::Closure* done){
        uint32_t id = request->id();
        std::string name = request->name();
        std::string pwd = request->pwd();

        bool ret = Register(id, name, pwd);

        response->mutable_result()->set_errcode(0);
        response->mutable_result()->set_errmsg("");
        response->set_sucess(ret);

        done->Run();
    }
};

int main(int argv, char** argc){

    //  调用框架的初始化操作   
    MprpcApplication::Init(argv, argc);

    //  provider是一个PRC网络服务对象。把UserService对象发布到RPC结点上
    RpcProvider provider;
    provider.NotifyService(new UserService());

    //  启动一个PRC服务结点 Run以后，进程进入阻塞状态，等待远程的RPC调用请求
    provider.Run();
    return 0;
}