#pragma once
#include <unistd.h>
#include <sys/types.h>
#include <functional>


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