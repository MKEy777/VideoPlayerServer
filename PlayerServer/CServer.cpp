#include "CServer.h"
#include "Logger.h"

CServer::CServer()
{
    m_server = nullptr;
    m_business = nullptr;
}

int CServer::Init(CBusiness* business, const Buffer& ip, short port)
{
    if (business == nullptr) return -1;
    m_business = business;

    int ret = 0;
    // [步骤 1]: 注册子进程的入口点
    ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess, m_business,&m_process);
    if (ret != 0) return -2;

	// [步骤 2]: 创建子进程,子进程进入业务循环
    ret = m_process.CreateSubProcess();
    if (ret != 0) return -3;

	// [步骤 3]: 启动线程池
    ret = m_pool.Start(2);
    if (ret != 0) return -4;

	// [步骤 4]: 初始化 Epoll 多路复用器
    ret = m_epoll.Create(2);
    if (ret != 0) return -5;

    // [步骤 5]: 构建并初始化服务端监听 Socket
    m_server = new CSocket();
    if (m_server == nullptr) return -6;
    ret = m_server->Init(CSockParam(ip, port, SOCK_ISSERVER | SOCK_ISIP | SOCK_ISNONBLOCK| SOCK_ISREUSE));
    if (ret != 0) return -7;

	// [步骤 6]: 将服务端 Socket 添加到 Epoll 监听列表，等待连接事件
    ret = m_epoll.Add(*m_server, EpollData((void*)m_server));
    if (ret != 0) return -8;

	// [步骤 7]: 启动线程池工作线程，处理 Epoll 事件
    for (size_t i = 0; i < m_pool.Size(); i++) {
        ret = m_pool.AddTask(&CServer::ThreadFunc, this);
        if (ret != 0) return -9;
    }

    return 0;
}

int CServer::Run()
{
    //TODO:std::condition_variable
    while (m_server != nullptr) {
        usleep(10);
    }
    return 0;
}

int CServer::Close()
{
    //停止Run() 和 ThreadFunc()
    if (m_server) {
        CSocketBase* sock = m_server;
        m_server = nullptr;

        m_epoll.Del(*sock);
        delete sock;
    }
	//关闭子进程和线程池
    m_epoll.Close();
    m_process.SendFD(-1);
    m_pool.Close();
    return 0;
}

int CServer::ThreadFunc()
{
    TRACEI("epoll %d server %p", (int)m_epoll, m_server);
    EPEvents events;

    while ((m_epoll != -1) && (m_server != nullptr)) {
        ssize_t size = m_epoll.WaitEvents(events,500);
        if (size < 0) break;
      
        for (ssize_t i = 0; i < size; i++) {
            //TRACEI("size=%d event %08X", (int)size, events[0].events);
            if (events[i].events & EPOLLERR) {
                break;
            }
            //处理可读事件
            if (events[i].events & EPOLLIN) {
                if (!m_server) continue;

                CSocketBase* pClient = nullptr;
                int ret = m_server->Link(&pClient);//// 执行 Accept 取出新连接
                if (ret != 0 || pClient == nullptr) continue;

                int client_fd = (int)(*pClient);// 获取底层套接字句柄

                // 【核心跨进程移交流程】
				//将句柄发送给子进程,进行业务处理，父进程不再管理该连接
                ret = m_process.SendSocket(*pClient, *pClient);
                delete pClient;
                pClient = nullptr;

                if (ret != 0) {
                    TRACEE("send client %d failed!", client_fd);
                    continue;
                }
            }
        }
    }
    TRACEI("服务器终止");
    return 0;
}
