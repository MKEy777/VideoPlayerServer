#pragma once
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <map>
#include "Function.h"
#include <cstdio>
#include <errno.h>

class CThread
{
public:
    CThread()
    {
        m_function = NULL;     // 线程执行函数对象
        m_thread = 0;          // pthread 线程ID，0表示未创建
        m_bpaused = false;     // 暂停标志
    }

    // 构造时绑定线程执行函数
    template<typename _FUNCTION_, typename... _ARGS_>
    CThread(_FUNCTION_ func, _ARGS_... args)
        : m_function(new CFunction<_FUNCTION_, _ARGS_...>(func, args...))
    {
        m_thread = 0;
        m_bpaused = false;
    }

    ~CThread() {}

public:
    CThread(const CThread&) = delete;
    CThread operator=(const CThread&) = delete;

public:
    // 设置线程执行函数
    template<typename _FUNCTION_, typename... _ARGS_>
    int SetThreadFunc(_FUNCTION_ func, _ARGS_... args)
    {
        m_function = new CFunction<_FUNCTION_, _ARGS_...>(func, args...);
        if (m_function == NULL) return -1;
        return 0;
    }

    // 创建线程
    int Start()
    {
        if (m_thread != 0) return -6;
        pthread_attr_t attr;//线程的“配置结构体”
        int ret = 0;

        ret = pthread_attr_init(&attr);
        if (ret != 0) return -1;

        // 设置为可 join 状态
        ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        if (ret != 0) return -2;

        // 设置线程作用域
        ret = pthread_attr_setscope(&attr, PTHREAD_SCOPE_PROCESS);
        if (ret != 0) return -3;

        // 创建线程，入口为静态函数
        /*
        pthread_create(
            pthread_t* thread,            //保存线程ID
            const pthread_attr_t* attr,   //线程属性
            void* (*start_routine)(void*),//线程入口函数
            void* arg                     //传给线程的参数
        );
        */
        ret = pthread_create(&m_thread, &attr, &CThread::ThreadEntry, this);
        if (ret != 0) return -4;

        // 记录线程ID与对象的映射（用于信号回调中找到对象）
        m_mapThread[m_thread] = this;

        ret = pthread_attr_destroy(&attr);
        if (ret != 0) return -5;

        return 0;
    }

    // 发送 SIGUSR1 控制线程暂停/恢复
    int Pause()
    {
        if (m_thread == 0) return -1;

        // 如果已经暂停，则恢复
        if (m_bpaused)
        {
            m_bpaused = false;
            return 0;
        }

        // 设置暂停标志并发送信号
        m_bpaused = true;
        int ret = pthread_kill(m_thread, SIGUSR1);//向指定线程发送一个信号
        if (ret != 0)
        {
            m_bpaused = false;
            return -2;
        }

        return 0;
    }

    // 停止线程
    int Stop()
    {
        if (m_thread != 0)
        {
            pthread_t thread = m_thread;
            m_thread = 0;   // 标记线程即将结束

            timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 100 * 1000000; // 等待100ms

            // 带超时的 join（GNU 扩展）
            int ret = pthread_timedjoin_np(thread, NULL, &ts);//等待线程结束
            if (ret == ETIMEDOUT)
            {
                // 超时则分离并强制发送退出信号
                pthread_detach(thread);
                pthread_kill(thread, SIGUSR2);//向指定线程发送一个信号
            }
        }
        return 0;
    }

    // 判断线程是否未运行
    bool isValid() const
    {
        return m_thread == 0;
    }
private:
    // 线程入口函数（静态）
    static void* ThreadEntry(void* arg)
    {
        CThread* thiz = (CThread*)arg;

        // 注册信号处理函数
        struct sigaction act = { 0 };
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_SIGINFO;
        act.sa_sigaction = &CThread::Sigaction;

        sigaction(SIGUSR1, &act, NULL); // 暂停控制
        sigaction(SIGUSR2, &act, NULL); // 强制退出

        // 执行用户函数
        thiz->EnterThread();

        // 线程结束时清理
        if (thiz->m_thread)
            thiz->m_thread = 0;

        pthread_t thread = pthread_self();

        // 清除映射
        auto it = m_mapThread.find(thread);
        if (it != m_mapThread.end())
            m_mapThread[thread] = NULL;

        pthread_detach(thread);
        pthread_exit(NULL);
    }

    // 信号回调函数
    //signo:信号编号 info:信号来源的详细信息 context:执行现场上下文
    static void Sigaction(int signo, siginfo_t* info, void* context)
    {
        if (signo == SIGUSR1)// 收到暂停信号
        {
            pthread_t thread = pthread_self();
            auto it = m_mapThread.find(thread);

            if (it != m_mapThread.end())
            {
                if (it->second)
                {
                    // 进入暂停自旋
                    while (it->second->m_bpaused)
                    {
                        // 若线程被要求结束，则退出
                        if (it->second->m_thread == 0)
                        {
                            pthread_exit(NULL);
                        }
                        usleep(1000); // 1ms 轮询
                    }
                }
            }
        }
        else if (signo == SIGUSR2)// 强制退出信号
        {
            pthread_exit(NULL);
        }
    }

    // 实际执行函数
    void EnterThread()
    {
        if (m_function != NULL)
        {
            int ret = (*m_function)();
            if (ret != 0)
            {
                printf("%s(%d):[%s]ret = %d\n",__FILE__, __LINE__, __FUNCTION__, ret);
            }
        }
    }
private:
    CFunctionBase* m_function;               // 封装的线程执行函数
    pthread_t m_thread;                      // 线程ID
    bool m_bpaused;                          // 暂停标志
    static std::map<pthread_t, CThread*> m_mapThread; // 线程ID到对象的映射表
};
