#pragma once

#include "Logger.h"
#include "CServer.h"
#include <map>
#include <cerrno>
#include <cstring>
#include <functional>

/*
* 1. 客户端的地址问题
* 2. 连接回调的参数问题
* 3. 接收回调的参数问题
*/

#define ERR_RETURN(ret, err) if(ret!=0){TRACEE("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));return err;}

#define WARN_CONTINUE(ret) if(ret!=0){TRACEW("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));continue;}


class CPlayerServer : public CBusiness
{
public:
    explicit CPlayerServer(unsigned count)
        : CBusiness(), m_count(count){}

    ~CPlayerServer(){
        m_epoll.Close();
        m_pool.Close();
        for (auto& it : m_mapClients) {
            if (it.second) {
                delete it.second;
            }
        }
        m_mapClients.clear();
    }

    virtual int BusinessProcess(CProcess* proc) {
        using namespace std::placeholders;
        int ret = 0; 
        ret = setConnectedCallback(&CPlayerServer::Connected, this, _1);
        ERR_RETURN(ret, -1);
        ret = setRecvCallback(&CPlayerServer::Received, this, _1, _2);
        ERR_RETURN(ret, -2);
        ret = m_epoll.Create(m_count);
        ERR_RETURN(ret, -1);
        ret = m_pool.Start(m_count);
        ERR_RETURN(ret, -2);
        for (unsigned i = 0; i < m_count; i++) {
            ret = m_pool.AddTask(&CPlayerServer::ThreadFunc, this);
            ERR_RETURN(ret, -3);
        }
        int sock = 0;
        sockaddr_in addrin;
        while (m_epoll != -1) {
            ret = proc->RecvSocket(sock, &addrin);
            if (ret < 0 || (sock == 0))break;
            CSocketBase* pClient = new CSocket(sock);
            if (pClient == NULL)continue;
            ret = pClient->Init(CSockParam(&addrin, SOCK_ISIP));
            WARN_CONTINUE(ret);
            ret = m_epoll.Add(sock, EpollData((void*)pClient));
            if (m_connectedcallback) {
                (*m_connectedcallback)(pClient);
            }
            WARN_CONTINUE(ret);
        }
        return 0;
    }

private:
    int Connected(CSocketBase* pClient) {
        printf("Server Connected: %d\n", (int)(*pClient));
        return 0;
    }
    int Received(CSocketBase* pClient, const Buffer& data) {
        printf("Server Recv: %s\n", (const char*)data);
        return 0;
    }

private:
    int ThreadFunc()
    {
        int ret = 0;
        EPEvents events;

        while (m_epoll != -1) {
            ssize_t size = m_epoll.WaitEvents(events);
            if (size < 0) break;

            for (ssize_t i = 0; i < size; i++) {
                CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
                //printf("Event Triggered! ptr=%p events=%d\n", pClient, events[i].events);
                if (events[i].events & EPOLLERR) {
                    //printf("!! EPOLLERR detected on %p. Closing.\n", pClient);
                    if (pClient) {
                        m_epoll.Del(*pClient);
                        delete pClient;
                    }
                    continue;
                }

                if (events[i].events & EPOLLIN) {
                    if (!pClient) continue;

                    Buffer data(4096);
                    ret = pClient->Recv(data);
                    //printf("Recv result: ret=%d errno=%d\n", ret, errno);
                    if (ret > 0) {
                        //printf(">> Server Recv Success: %d bytes. Data: [%s]\n", ret, (char*)data);
                        if (m_recvcallback) {
                            (*m_recvcallback)(pClient, data);
                        }
                        ret = 0;
                    }
                    else if (ret < 0) {
                        //printf("!! Recv Failed. ret=%d errno=%d msg=%s\n", ret, errno, strerror(errno));
                        m_epoll.Del(*pClient);
                        delete pClient;
                    }
                    else {
                        //printf("Recv EAGAIN (No data yet).\n");
                    }

                    WARN_CONTINUE(ret);
                }
            }
        }

        return 0;
    }

private:
    CEpoll m_epoll;
    std::map<int, CSocketBase*> m_mapClients;
    CThreadPool m_pool;
    unsigned m_count = 0;
};
