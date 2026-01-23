#include <iostream>
#include <unistd.h>
#include <functional>
#include <type_traits>

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
    CProcess() : m_func(nullptr), m_pid(-1) {}

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
        pid_t pid = fork();
        if (pid == -1) return -2;
        if (pid == 0) {
            // 子进程执行业务逻辑
            int ret = (*m_func)();
            _exit(ret);
        }
        m_pid = pid;
        return 0;
    }

private:
    CFunctionBase* m_func;
    pid_t m_pid;
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
    proclog.CreateSubProcess();

    proccliets.SetEntryFunction(CreateClientServer, &proccliets);
    proccliets.CreateSubProcess();
    //生产环境下这里通常需要 wait() 子进程，否则主进程退出后子进程会托管给 init
    return 0;
}