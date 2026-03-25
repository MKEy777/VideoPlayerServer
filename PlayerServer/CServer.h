#pragma once

#include "Socket.h"
#include "Epoll.h"
#include "ThreadPool.h"
#include "Process.h"
#include "Function.h"

/**
 * @brief 业务逻辑处理的抽象基类
 * * @details
 * [架构职责]: 本类是网络通信层与上层业务的解耦接口。
 * [运行环境]: 本类的实际业务代码（BusinessProcess）将运行在子进程中。
 * [资源管理]: 由调用者分配，并在服务端关闭时手动释放。
 */
class CBusiness
{
public:
    virtual ~CBusiness() = default;

    /**
     * @brief 子进程业务的主循环/入口函数
     * @param proc 指向当前跨进程通信对象的指针，用于读取主进程发来的 FD 或命令
     */
    virtual int BusinessProcess(CProcess* proc) = 0;

    //注册“客户端连接成功”的异步回调事件
    template <typename _FUNCTION_, typename... _ARGS_>
    int setConnectedCallback(_FUNCTION_ func, _ARGS_... args)
    {
        delete m_connectedcallback;
        m_connectedcallback = new CConnectedFunction<_FUNCTION_, _ARGS_...>(func, args...);
        if (m_connectedcallback == nullptr) return -1;
        return 0;
    }

    //注册“收到网络数据”的异步回调事件
    template <typename _FUNCTION_, typename... _ARGS_>
    int setRecvCallback(_FUNCTION_ func, _ARGS_... args)
    {
        delete m_recvcallback;
        m_recvcallback = new CRecvFunction<_FUNCTION_, _ARGS_...>(func, args...);
        if (m_recvcallback == nullptr) return -1;
        return 0;
    }

protected:
    CBusiness() = default;
    CFunctionBase* m_connectedcallback = nullptr;
    CFunctionBase* m_recvcallback = nullptr;
};


/**
 * @brief 服务端核心基座
 * * @details
 * [架构模型]: 多线程 Epoll 监听 + 多进程 FD 透传模型。
 * [工作流程]:
 * 1. 主进程初始化线程池与 Epoll，绑定 Server Socket。
 * 2. 主进程 Fork 子进程，子进程进入 CBusiness::BusinessProcess 挂起。
 * 3. 客户端发起 TCP 握手，主进程的 Epoll 触发，执行 Accept (Link)。
 * 4. 主进程通过 CProcess 将新生 Socket 的文件描述符 (FD) 跨进程发给子进程。
 * 5. 主进程销毁 Socket 对象（但不关底层FD），子进程接管连接的数据读写。
 */
class CServer
{
public:
    CServer();
    ~CServer() { Close(); }

    CServer(const CServer&) = delete;
    CServer& operator=(const CServer&) = delete;
public:
    //初始化并启动服务端环境
    int Init(CBusiness* business, const Buffer& ip = "0.0.0.0", short port = 9999);
    //阻塞主线程，维持服务运行
    int Run();
    //释放 Socket，清理 Epoll，向子进程发送退出信号，并安全关闭线程池
    int Close();
private:
    //线程池工作线程的主函数
    int ThreadFunc();
private:
    CThreadPool   m_pool;   // 并发线程池，处理 Epoll 事件
    CSocketBase*  m_server = nullptr;// 服务端监听套接字指针
    CEpoll        m_epoll;  // 负责 I/O 事件监听
    CProcess      m_process;//IPC 进程通信管理器，传递 Fork 和 FD 
    CBusiness*    m_business = nullptr; // 业务模块,手动 delete
};
