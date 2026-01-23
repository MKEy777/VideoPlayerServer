#include <iostream>
#include <unistd.h>
#include <functional>
#include <type_traits>
#include <memory.h>
#include <sys/types.h>
#include <sys/socket.h>

class CFunctionBase {
public:
    virtual ~CFunctionBase() {}
    virtual int operator()() = 0;
};

template <typename _FUNCTION_, typename... _ARGS_>
class CFunction : public CFunctionBase {
public:
    // 构造函数：使用 std::bind 绑定函数和参数
    CFunction(_FUNCTION_ func, _ARGS_... args)
        : m_binder(std::bind(std::move(func), std::move(args)...)) {
    }

    int operator()() override {
        return m_binder();
    }
private:
    std::function<int()> m_binder;
};

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
    void SetEntryFunction(_FUNCTION_&& func, _ARGS_&&... args) {
        // 如果多次调用 SetEntryFunction，先清理旧的
        if (m_func) {
            delete m_func;
        }

        // 1. 使用 std::decay 处理类型，防止引用退化导致的悬空指针
        // 2. 使用原始指针 new 分配对象
        typedef CFunction<
            typename std::decay<_FUNCTION_>::type,
            typename std::decay<_ARGS_>::type...
        > ConcreteFunction;

        m_func = new ConcreteFunction(
            std::forward<_FUNCTION_>(func),
            std::forward<_ARGS_>(args)...
        );
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
            pipes[1] = 0;
            // 子进程只用 pipes[0] 接收来自父进程的信息
            _exit((*m_func)());
        }
		// 父进程
        close(pipes[0]);
        pipes[0] = 0;
        // 父进程只用 pipes[1] 向子进程发送信息
        m_pid = pid;
        return 0;
    }

    int SendFD(int fd) {
        struct msghdr msg;
        iovec iov[2];
		iov[0].iov_base = (char*)"F";
		iov[0].iov_len = 1;
        iov[1].iov_base = (char*)"F";
        iov[1].iov_len = 1;
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        // 发送文件描述符
		cmsghdr* cmsg = (cmsghdr*)calloc(1,CMSG_SPACE(sizeof(int)));
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		*((int*)CMSG_DATA(cmsg)) = fd;
        msg.msg_control = cmsg;
		msg.msg_controllen = cmsg->cmsg_len;
		ssize_t ret = sendmsg(pipes[1], &msg, 0);
        if (ret == -1) {
            return -1;
        }
        free(cmsg);
        return 0;
    }

    int RecvFD(int& fd) {
        struct msghdr msg;
        iovec iov[2];
		char buf[][10] = { "","" };
        iov[0].iov_base = buf[0];
        iov[0].iov_len = sizeof(buf[0]);
        iov[1].iov_base = buf[1];
        iov[1].iov_len = sizeof(buf[1]);
        msg.msg_iov = iov;
        msg.msg_iovlen = 2;

        cmsghdr* cmsg = (cmsghdr*)calloc(1, CMSG_SPACE(sizeof(int)));
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        msg.msg_control = cmsg;
        msg.msg_controllen = cmsg->cmsg_len;
        ssize_t ret = recvmsg(pipes[0], &msg, 0);
        if (ret == -1) {
            free(cmsg);
            return -1;
        }
		fd = *(int*)CMSG_DATA(cmsg);
        return 0;
    }

private:
    CFunctionBase* m_func;
    pid_t m_pid;
    int pipes[2];
};

// --- 业务代码 ---

int CreateLogServer(CProcess* proc) {
    printf("Log Server Running...\n");
    return 0;
}

int CreateClientServer(CProcess* proc) {
    printf("Client Server Running...\n");
    return 0;
}

int main() {
    CProcess proclog, proccliets;

    proclog.SetEntryFunction(CreateLogServer, &proclog);
    int ret = proclog.CreateSubProcess();
    if(ret!=0) {
        //std::cout << -1 << std::endl;
        return -1;
	}

    proccliets.SetEntryFunction(CreateClientServer, &proccliets);
    ret= proccliets.CreateSubProcess();
    if (ret != 0) {
        //std::cout << -2 << std::endl;
        return -2;
    }
    //生产环境下这里通常需要 wait() 子进程，否则主进程退出后子进程会托管给 init
    return 0;
}