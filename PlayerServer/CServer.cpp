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

    ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess, m_business,&m_process);
    if (ret != 0) return -2;

    ret = m_process.CreateSubProcess();
    if (ret != 0) return -3;

    ret = m_pool.Start(2);
    if (ret != 0) return -4;

    ret = m_epoll.Create(2);
    if (ret != 0) return -5;

    m_server = new CSocket();
    if (m_server == nullptr) return -6;

    ret = m_server->Init(CSockParam(ip, port, SOCK_ISSERVER | SOCK_ISIP | SOCK_ISNONBLOCK));
    if (ret != 0) return -7;

    ret = m_epoll.Add(*m_server, EpollData((void*)m_server));
    if (ret != 0) return -8;

    for (size_t i = 0; i < m_pool.Size(); i++) {
        ret = m_pool.AddTask(&CServer::ThreadFunc, this);
        if (ret != 0) return -9;
    }

    return 0;
}

int CServer::Run()
{
    while (m_server != nullptr) {
        usleep(10);
    }
    return 0;
}

int CServer::Close()
{
    if (m_server) {
        CSocketBase* sock = m_server;
        m_server = nullptr;

        m_epoll.Del(*sock);
        delete sock;
    }

    m_epoll.Close();
    m_process.SendFD(-1);
    m_pool.Close();
    return 0;
}

int CServer::ThreadFunc()
{
    EPEvents events;

    while ((m_epoll != -1) && (m_server != nullptr)) {
        ssize_t size = m_epoll.WaitEvents(events);
        if (size < 0) break;

        for (ssize_t i = 0; i < size; i++) {
            if (events[i].events & EPOLLERR) {
                break;
            }

            if (events[i].events & EPOLLIN) {
                if (!m_server) continue;

                CSocketBase* pClient = nullptr;
                int ret = m_server->Link(&pClient);
                if (ret != 0 || pClient == nullptr) continue;

                int client_fd = (int)(*pClient);

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

    return 0;
}
