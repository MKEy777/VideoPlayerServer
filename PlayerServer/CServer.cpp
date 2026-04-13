#include "CServer.h"
#include "Logger.h"

CServer::CServer()
{
    m_server = nullptr;
    m_business = nullptr;
    m_runtimeConfig = NetRuntimeConfig::Default();
}

int CServer::Init(CBusiness* business, const Buffer& ip, short port)
{
    return Init(business, NetRuntimeConfig::Default(), ip, port);
}

int CServer::Init(CBusiness* business, const NetRuntimeConfig& cfg, const Buffer& ip, short port)
{
    if (business == nullptr) return -1;
    m_business = business;
    m_runtimeConfig = cfg;

    int ret = 0;
    ret = m_process.SetEntryFunction(&CBusiness::BusinessProcess, m_business, &m_process);
    if (ret != 0) return -2;

    ret = m_process.CreateSubProcess();
    if (ret != 0) return -3;

    ret = m_pool.Start(2);
    if (ret != 0) return -4;

    ret = m_epoll.Create(2);
    if (ret != 0) return -5;

    m_server = new CSocket();
    if (m_server == nullptr) return -6;

    ret = m_server->Init(CSockParam(ip, port, SOCK_ISSERVER | SOCK_ISIP | SOCK_ISNONBLOCK | SOCK_ISREUSE));
    if (ret != 0) return -7;

    ret = m_epoll.Add(*m_server, EpollData((void*)m_server));
    if (ret != 0) return -8;

    for (size_t i = 0; i < m_pool.Size(); i++) {
        ret = m_pool.AddTask(&CServer::ThreadFunc, this);
        if (ret != 0) return -9;
    }

    const bool wantsDpdk =
        (m_runtimeConfig.mode == NetMode::DpdkLearn) && m_runtimeConfig.dpdkEnabled;

    if (wantsDpdk) {
        const int dpdkRet = m_dpdkRuntime.Start(m_runtimeConfig);
        if (dpdkRet != 0) {
            TRACEW(
                "DPDK start failed (ret=%d). Fallback to epoll-only mode. cfg=[%s]",
                dpdkRet,
                (const char*)m_runtimeConfig.ToString());
            m_runtimeConfig.mode = NetMode::EpollOnly;
            m_runtimeConfig.dpdkEnabled = false;
        }
    }

    TRACEI("server runtime config: %s", (const char*)m_runtimeConfig.ToString());
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
    m_dpdkRuntime.Stop();

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
    TRACEI("epoll=%d server=%p", (int)m_epoll, m_server);
    EPEvents events;

    while ((m_epoll != -1) && (m_server != nullptr)) {
        ssize_t size = m_epoll.WaitEvents(events, 500);
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
                    TRACEE("send client %d to business process failed!", client_fd);
                    continue;
                }
            }
        }
    }
    TRACEI("server thread exited");
    return 0;
}

