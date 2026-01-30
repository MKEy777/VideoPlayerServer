#pragma once
#include "Thread.h"
#include "Epoll.h"
#include "Socket.h"

#include <list>
#include <map>
#include <sstream>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/timeb.h>

/* ================= 日志级别 ================= */

enum LogLevel {
    LOG_INFO,
    LOG_DEBUG,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL
};

/* ================= LogInfo ================= */

class LogInfo {
public:
    LogInfo(//格式化构造函数
        const char* file,
        int line,
        const char* func,
        pid_t pid,
        pthread_t tid,
        int level,
        const char* fmt,
        ...
    );

    LogInfo(//流式构造函数
        const char* file,
        int line,
        const char* func,
        pid_t pid,
        pthread_t tid,
        int level
    );

	LogInfo(//二进制数据构造函数
        const char* file,
        int line,
        const char* func,
        pid_t pid,
        pthread_t tid,
        int level,
        void* pData,
        size_t nSize
    );

    ~LogInfo();

    operator Buffer() const {return m_buf;}

    template<typename T>//将任意类型 T 转换为字符串。
    LogInfo& operator<<(const T& data) {
        std::stringstream stream;
        stream << data;
        m_buf += stream.str();
        return *this;
    }

private:
    bool   bAuto;   // 默认 false，流式日志为 true
    Buffer m_buf;
};

/* ================= 日志服务器 ================= */

class CLoggerServer {
public:
    CLoggerServer()
        : m_thread(&CLoggerServer::ThreadFunc, this) // 日志线程：后台写文件
        , m_server(nullptr)                          // 本地socket服务端指针
    {
        // 生成日志文件路径：./log/<时间>.log
        m_path = Buffer(std::string("./log/") + GetTimeStr().c_str() + ".log");

        // 输出当前日志文件路径
        printf("%s(%d):[%s] path=%s\n",
            __FILE__, __LINE__, __FUNCTION__, (char*)m_path);
    }
    ~CLoggerServer() {Close();} // 析构时关闭 server/epoll/线程

    CLoggerServer(const CLoggerServer&) = delete;
    CLoggerServer& operator=(const CLoggerServer&) = delete;
public:
    // 启动日志服务器（创建目录/文件/epoll/socket/线程）
    int Start() {
        if (m_server != nullptr) // 防止重复启动
            return -1;

        // 确保 log 目录存在且可读写，不存在则创建
        if (access("log", W_OK | R_OK) != 0) {
            mkdir("log", S_IRUSR | S_IWUSR | S_IXUSR |
                S_IRGRP | S_IWGRP | S_IXGRP |
                S_IROTH | S_IXOTH);
        }

        // 打开日志文件（写入并可读）
        m_file = fopen(m_path, "w+");
        if (!m_file)
            return -2;

        // 创建 epoll（用于监听 server socket 和 client socket 的可读事件）
        if (m_epoll.Create(1) != 0) return -3;

        // 创建本地 socket 服务端对象
        m_server = new CLocalSocket();
        if (!m_server) {
            Close();
            return -4;
        }

        // 初始化服务端 socket（绑定 ./log/server.sock 并监听）
        if (m_server->Init(CSockParam("./log/server.sock",
            (int)SOCK_ISSERVER)) != 0) {
            Close();
            return -5;
        }

        if (m_epoll.Add(*m_server, EpollData(m_server), EPOLLIN | EPOLLERR) != 0) {
            Close();
            return -7;
        }

        // 启动日志线程：进入 ThreadFunc epoll 循环
        if (m_thread.Start() != 0) {
            Close();
            return -6;
        }

        return 0;
    }
    // 日志线程函数：epoll 等待连接/接收日志并写入文件
    int ThreadFunc() {
        EPEvents events; // epoll 返回的事件数组/容器
        std::map<int, CSocketBase*> mapClients; // 保存所有已连接的客户端（key 是 fd）

        // 主循环：线程有效 + epoll 正常 + server 存在
        while (m_thread.isValid() && m_server) {

            // 等待事件（timeout=1）
            ssize_t ret = m_epoll.WaitEvents(events, 1);
            if (ret < 0)
                break;

            // 处理本轮所有触发的事件
            for (ssize_t i = 0; i < ret; ++i) {
                // 发生错误事件：直接跳出（一般意味着 fd 异常）
                if (events[i].events & EPOLLERR) {
                    break;
                }

                // 可读事件
                if (events[i].events & EPOLLIN) {
                    // 如果是服务端 socket 可读：表示有新连接进来（accept）
                    if (events[i].data.ptr == m_server) {
                        CSocketBase* pClient = nullptr;
                        // Link：服务端 accept 出一个新客户端连接对象
                        if (m_server->Link(&pClient) < 0)
                            continue;
                        // 将新客户端 fd 加入 epoll 监听（关注可读和错误）
                        if (m_epoll.Add(*pClient,
                            EpollData(pClient), // ptr 存放连接对象指针，方便回调时拿到
                            EPOLLIN | EPOLLERR) < 0) {
                            delete pClient;
                            continue;
                        }
                        // 保存到 map，便于统一释放/管理
                        mapClients[*pClient] = pClient;
                    }
                    else {
                        // 普通客户端 socket 可读
                        CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;

                        Buffer data(1024 * 1024);
                        data.resize(1024 * 1024); // 关键修复

                        int r = pClient->Recv(data);

                        // === 调试打印 ===
                        printf("[Debug] Recv fd=%d, ret=%d\n", (int)(*pClient), r);
                        // ===============

                        if (r <= 0) {
                            printf("[Debug] Client disconnected!\n");
                            delete pClient;
                            mapClients[*pClient] = nullptr;
                        }
                        else {
                            printf("[Debug] Log received: %s\n", data.data()); // 打印收到的内容
                            WriteLog(data);
                        }
                    }
                }
            }
        }

        // 线程退出：清理所有客户端连接
        for (auto& it : mapClients) {
            delete it.second;
        }
        mapClients.clear();

        return 0;
    }
    // 释放 server，关闭 epoll，停止线程
    int Close() {
        if (m_server) {
            delete m_server;
            m_server = nullptr;
        }
        if (m_file) {
            fclose(m_file);
            m_file = nullptr;
        }
        m_epoll.Close();
        m_thread.Stop();
        return 0;
    }
public:
    // 供其他线程/进程调用：把 LogInfo 通过本地 socket 发给日志服务器
    static void Trace(const LogInfo& info) {
        static thread_local CLocalSocket client; // 每线程一个连接，避免锁竞争
        // 若当前线程还没连接 server.sock，则先 Init 连接
        if (client == -1) {
            if (client.Init(CSockParam("./log/server.sock", 0)) != 0) {
//#ifdef _DEBUG
                printf("%s(%d):[%s] socket init failed\n",
                    __FILE__, __LINE__, __FUNCTION__);
//#endif
                return;
            }
            if (client.Link() != 0) {
                printf("%s(%d):[%s] Connect LogServer failed!\n", __FILE__, __LINE__, __FUNCTION__);
                client.Close(); // 连接失败需清理
                return;
            }
        }

        // 发送日志内容到日志服务器（info 可隐式转 Buffer）
        client.Send(info);
    }
    // 获取当前时间字符串：用于日志文件名/日志头等
    static Buffer GetTimeStr() {
        Buffer result(128);
        timeb tmb;
        ftime(&tmb); // 毫秒级时间（tmb.time 秒 + tmb.millitm 毫秒）
        tm tmv{};
        localtime_r(&tmb.time, &tmv); // Linux 线程安全：把本地时间写入 tmv
       
        int nSize = snprintf( // 格式：YYYY-MM-DD HH-MM-SS mmm
            result,
            128,
            "%04d-%02d-%02d %02d-%02d-%02d %03d",
            tmv.tm_year + 1900,
            tmv.tm_mon + 1,
            tmv.tm_mday,
            tmv.tm_hour,
            tmv.tm_min,
            tmv.tm_sec,
            tmb.millitm
        );
        result.resize(nSize); // 设置有效长度
        return result;
    }
private:
    // 将接收到的日志内容写入文件（并 flush）
    void WriteLog(const Buffer& data) {
        if (!m_file) return;

        // 按 Buffer 的有效长度写入
        fwrite((const char*)data, 1, data.size(), m_file);
        // 立即刷新到磁盘（安全但高频会影响性能）
        fflush(m_file);

#ifdef _DEBUG
        // 调试输出到控制台（假设 data 可当作 C 字符串）
        printf("%s", data.data());
#endif
    }

private:
    CThread      m_thread;   // 后台日志线程
    CEpoll       m_epoll;    // epoll 事件复用
    CSocketBase* m_server;   // 本地 socket 服务端（接受连接）
    Buffer       m_path;     // 日志文件路径
    FILE*        m_file;     // 日志文件句柄
};

/* ================= 宏定义 ================= */

#ifndef TRACE

#define TRACEI(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, \
    getpid(), pthread_self(), LOG_INFO, __VA_ARGS__))

#define TRACED(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, \
    getpid(), pthread_self(), LOG_DEBUG, __VA_ARGS__))

#define TRACEW(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, \
    getpid(), pthread_self(), LOG_WARNING, __VA_ARGS__))

#define TRACEE(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, \
    getpid(), pthread_self(), LOG_ERROR, __VA_ARGS__))

#define TRACEF(...) CLoggerServer::Trace(LogInfo(__FILE__, __LINE__, __FUNCTION__, \
    getpid(), pthread_self(), LOG_FATAL, __VA_ARGS__))

// 流式日志operator<<
#define LOGI LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO)
#define LOGD LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG)
#define LOGW LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING)
#define LOGE LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR)
#define LOGF LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL)

// 内存十六进制 dump
#define DUMPI(data, size) LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_INFO, data, size)
#define DUMPD(data, size) LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_DEBUG, data, size)
#define DUMPW(data, size) LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_WARNING, data, size)
#define DUMPE(data, size) LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_ERROR, data, size)
#define DUMPF(data, size) LogInfo(__FILE__, __LINE__, __FUNCTION__, getpid(), pthread_self(), LOG_FATAL, data, size)

#endif