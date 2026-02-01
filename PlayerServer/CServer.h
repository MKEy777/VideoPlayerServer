#pragma once

#include "Socket.h"
#include "Epoll.h"
#include "ThreadPool.h"
#include "Process.h"
#include "Function.h"

// 业务接口：由使用者实现
class CBusiness
{
public:
    virtual ~CBusiness() = default;
    virtual int BusinessProcess(CProcess* proc) = 0;

    template <typename _FUNCTION_, typename... _ARGS_>
    int setConnectedCallback(_FUNCTION_ func, _ARGS_... args)
    {
        delete m_connectedcallback;
        m_connectedcallback = new CConnectedFunction<_FUNCTION_, _ARGS_...>(func, args...);
        if (m_connectedcallback == nullptr) return -1;
        return 0;
    }

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

class CServer
{
public:
    CServer();
    ~CServer() { Close(); }

    CServer(const CServer&) = delete;
    CServer& operator=(const CServer&) = delete;
public:
    int Init(CBusiness* business, const Buffer& ip = "127.0.0.1", short port = 9999);
    int Run();
    int Close();
private:
    int ThreadFunc();
private:
    CThreadPool   m_pool;
    CSocketBase*  m_server = nullptr;
    CEpoll        m_epoll;
    CProcess      m_process;
    CBusiness* m_business = nullptr; // 业务模块,手动 delete
};
