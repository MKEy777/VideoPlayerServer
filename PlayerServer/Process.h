#pragma once
#include "Function.h"
#include <memory.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <cstdlib>

class CProcess {
public:
    CProcess() : m_func(nullptr), m_pid(-1) {
        memset(pipes, 0, sizeof(pipes));
    }

    // 手动管理内存：析构时释放原始指针
    ~CProcess() {
        if (m_func) {
            delete m_func;
            m_func = nullptr;
        }
    }

    template <typename _FUNCTION_, typename... _ARGS_>
    int SetEntryFunction(_FUNCTION_&& func, _ARGS_&&... args) {
        // 如果多次调用 SetEntryFunction，先清理旧的
        if (m_func) {
            delete m_func;
        }

        // 1. 使用 std::decay 处理类型，防止引用退化导致的悬空指针
        // 2. 使用原始指针 new 分配对象
        typedef CFunction<
            typename std::decay<_FUNCTION_>::type,
            typename std::decay<_ARGS_>::type...> ConcreteFunction;

        m_func = new ConcreteFunction(
            std::forward<_FUNCTION_>(func),
            std::forward<_ARGS_>(args)...);
        return 0;
    }

    int CreateSubProcess() {
        if (!m_func) return -1;
        int ret = socketpair(AF_LOCAL, SOCK_STREAM, 0, pipes);
        if (ret == -1) return -2;
        pid_t pid = fork();
        if (pid == -1) return -3;
        if (pid == 0) {
            // 子进程执行业务逻辑
            close(pipes[1]); // 关闭写
            pipes[1] = -1;
            // 子进程只用 pipes[0] 接收来自父进程的信息
            int ret = (*m_func)();
            _exit(ret);
        }
        // 父进程
        close(pipes[0]);
        pipes[0] = -1;
        // 父进程只用 pipes[1] 向子进程发送信息
        m_pid = pid;
        return 0;
    }

    int SendFD(int fd) {
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg)); // 必须初始化

        iovec iov[2];
        char buf[2][10] = { "edoyun", "jueding" };
        iov[0].iov_base = buf[0];
        iov[0].iov_len = sizeof(buf[0]);
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[1]);
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        size_t cm_len = CMSG_SPACE(sizeof(int));
        cmsghdr* cmsg = (cmsghdr*)calloc(1, cm_len);
        cmsg->cmsg_len = CMSG_LEN(sizeof(int)); // 数据净长
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        *((int*)CMSG_DATA(cmsg)) = fd;

        msg.msg_control = cmsg;
        msg.msg_controllen = cm_len; // 使用 SPACE 包含填充

        ssize_t ret = sendmsg(pipes[1], &msg, 0);
        free(cmsg);
        if (ret == -1) {
            return -2;
        }
        return 0;
    }

    int RecvFD(int& fd) {
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg));

        iovec iov[2];
        char buf[2][10];
        iov[0].iov_base = buf[0];
        iov[0].iov_len = sizeof(buf[0]);
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[1]);
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        size_t cm_len = CMSG_SPACE(sizeof(int));
        auto* cmsg = (cmsghdr*)calloc(1, cm_len);
        msg.msg_control = cmsg;
        msg.msg_controllen = cm_len;

        ssize_t ret = recvmsg(pipes[0], &msg, 0);
        if (ret <= 0) {
            free(cmsg);
            return -1;
        }

        struct cmsghdr* pcmsg = CMSG_FIRSTHDR(&msg);
        if (pcmsg && pcmsg->cmsg_type == SCM_RIGHTS) {
            fd = *(int*)CMSG_DATA(pcmsg);
        }
        else {
            free(cmsg);
            return -3;
        }

        free(cmsg);
        return 0;
    }

    static int SwitchDeamon() {
        pid_t ret = fork();
        if (ret == -1) return -1;
        if (ret > 0) exit(0);//主进程到此为止//子进程内容如下
        ret = setsid();
        if (ret == -1) return -2;//失败，则返回
        ret = fork();
        if (ret == -1) return -3;
        if (ret > 0) exit(0);//子进程到此为止
        //孙进程的内容如下，进入守护状态
        for (int i = 0; i < 3; i++)
            close(i);
        umask(0);
        signal(SIGCHLD, SIG_IGN);
        return 0;
    }

private:
    CFunctionBase* m_func;
    pid_t m_pid;
    int pipes[2];
};
