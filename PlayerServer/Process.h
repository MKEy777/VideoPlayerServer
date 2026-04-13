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
        memset(pipes,-1, sizeof(pipes));
    }

    // 手动管理内存：析构时释放原始指针
    ~CProcess() {
        if (m_func) {
            delete m_func;
            m_func = nullptr;
        }
        if (pipes[0] > 0) close(pipes[0]);
        if (pipes[1] > 0) close(pipes[1]);
    }

    template <typename _FUNCTION_, typename... _ARGS_>
    int SetEntryFunction(_FUNCTION_&& func, _ARGS_&&... args) {
        // 如果多次调用 SetEntryFunction，先清理旧的
        if (m_func) {delete m_func;}

        // 1. 使用 std::decay 处理类型，去除引用和 const 修饰符
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
        memset(&msg, 0, sizeof(msg)); // 初始化消息结构体

        // 普通数据区
        iovec iov[2];
        char buf[2][10] = { "e", "j" };//占位数据
        iov[0].iov_base = buf[0];
        iov[0].iov_len = sizeof(buf[0]);
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[1]);
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        // ----------- 控制消息区（传递fd）-----------
        size_t cm_len = CMSG_SPACE(sizeof(int)); // 包含对齐的总空间
        cmsghdr* cmsg = (cmsghdr*)calloc(1, cm_len);

        cmsg->cmsg_len = CMSG_LEN(sizeof(int)); // 实际数据长度（不含填充）
        cmsg->cmsg_level = SOL_SOCKET;          // socket 层
        cmsg->cmsg_type = SCM_RIGHTS;           // 表示传递“文件描述符”

        *((int*)CMSG_DATA(cmsg)) = fd;          // 把 fd 写入控制数据区

        msg.msg_control = cmsg;                 // 控制消息挂到 msg
        msg.msg_controllen = cm_len;            // 控制消息总长度

        // ----------- 发送消息（通过 UNIX 域 socket）-----------
        ssize_t ret = sendmsg(pipes[1], &msg, 0);
        free(cmsg);

        if (ret == -1) {
            return -2;
        }
        return 0;
    }

    int RecvFD(int& fd) {
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg)); // 初始化

        // ----------- 普通数据接收区-----------
        iovec iov[2];
        char buf[2][10];
        iov[0].iov_base = buf[0];
        iov[0].iov_len = sizeof(buf[0]);
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[1]);
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        // ----------- 控制消息接收区-----------
        size_t cm_len = CMSG_SPACE(sizeof(int));
        auto* cmsg = (cmsghdr*)calloc(1, cm_len);

        msg.msg_control = cmsg;
        msg.msg_controllen = cm_len;

        // 阻塞等待父进程发送（核心：recvmsg）
        ssize_t ret = recvmsg(pipes[0], &msg, 0);
        if (ret <= 0) {
            free(cmsg);
            return -1;
        }

        // ----------- 解析控制消息-----------
        struct cmsghdr* pcmsg = CMSG_FIRSTHDR(&msg); // 取第一个控制头
        if (pcmsg && pcmsg->cmsg_type == SCM_RIGHTS) {
            fd = *(int*)CMSG_DATA(pcmsg); // 取出传递的 fd
        }
        else {
            free(cmsg);
            return -3;
        }

        free(cmsg);
        return 0;
    }

    int SendSocket(int fd, const sockaddr_in* addrin) {//主进程完成
        struct msghdr msg;
        iovec iov;
        char buf[20] = "";
        bzero(&msg, sizeof(msg));

        //将客户端的 sockaddr_in传递
        memcpy(buf, addrin, sizeof(sockaddr_in));
        iov.iov_base = buf;
        iov.iov_len = sizeof(buf);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
        if (cmsg == NULL)return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        *(int*)CMSG_DATA(cmsg) = fd;
        msg.msg_control = cmsg;
        msg.msg_controllen = CMSG_SPACE(sizeof(int));

        ssize_t ret = sendmsg(pipes[1], &msg, 0);
        free(cmsg);
        if (ret == -1) {
            printf("********errno %d msg:%s\n", errno, strerror(errno));
            return -2;
        }
        return 0;
    }

    int RecvSocket(int& fd, sockaddr_in* addrin)
    {
        msghdr msg;
        iovec iov;
        char buf[20] = "";
        bzero(&msg, sizeof(msg));
        iov.iov_base = buf;
        iov.iov_len = sizeof(buf);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_LEN(sizeof(int)));
        if (cmsg == NULL)return -1;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        msg.msg_control = cmsg;
        msg.msg_controllen = CMSG_LEN(sizeof(int));
        ssize_t ret = recvmsg(pipes[0], &msg, 0);
        if (ret == -1) {
            free(cmsg);
            return -2;
        }
        memcpy(addrin, buf, sizeof(sockaddr_in));
        fd = *(int*)CMSG_DATA(cmsg);
        free(cmsg);
        return 0;
    }

    // 静态函数：让当前进程脱离终端控制，变成后台常驻守护进程
    static int SwitchDeamon() {
        // 第一步：第一次 fork
        pid_t ret = fork();
        if (ret == -1) return -1;
        if (ret > 0) exit(0); // 主进程直接退出。此时子进程会被 init/systemd 进程接管

        // 第二步：创建新的会话，脱离原控制终端
        ret = setsid();
        if (ret == -1) return -2;

        // 第三步：第二次 fork (防止进程再次打开控制终端)
        ret = fork();
        if (ret == -1) return -3;
        if (ret > 0) exit(0); // 子进程也退出

        // 孙进程(守护进程)
        // 关闭标准输入、标准输出、标准错误（0, 1, 2）
        for (int i = 0; i < 3; i++) close(i);

        // 重置文件权限掩码，使得它创建的文件具有最高权限
        umask(0);

        // 忽略子进程退出信号，防止产生僵尸进程
        signal(SIGCHLD, SIG_IGN);
        return 0;
    }

private:
    CFunctionBase* m_func;
    pid_t m_pid;//保存 fork()出来的子进程 ID
    int pipes[2];//存放 socketpair 创建的两个套接字句柄
};
