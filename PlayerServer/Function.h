#pragma once

#include <unistd.h>
#include <sys/types.h>
#include <functional>
#include <utility>

class CSocketBase;
class Buffer;

class CFunctionBase
{
public:
    virtual ~CFunctionBase() {}

    virtual int operator()() { return 0; }
    virtual int operator()(CSocketBase*) { return 0; }
    virtual int operator()(CSocketBase*, const Buffer&) { return 0; }
};

// 你指定的实现方式：统一用 std::function<int()> 保存 binder
template <typename _FUNCTION_, typename... _ARGS_>
class CFunction : public CFunctionBase
{
public:
    // 构造函数：使用 std::bind 绑定函数和参数
    CFunction(_FUNCTION_ func, _ARGS_... args)
        : m_binder(std::bind(std::move(func), std::move(args)...))
    {
    }

    int operator()() override
    {
        return m_binder();
    }

private:
    std::function<int()> m_binder;
};

// Connected 回调：对外仍然暴露 operator()(CSocketBase*)
template <typename _FUNCTION_, typename... _ARGS_>
class CConnectedFunction : public CFunctionBase
{
public:
    CConnectedFunction(_FUNCTION_ func, _ARGS_... args)
        : m_binder(std::bind(std::move(func), std::move(args)...))
    {
    }

    int operator()(CSocketBase* pClient) override
    {
        // 这里假定 bind 的目标最终能接受 (CSocketBase*) 参数
        return m_binder(pClient);
    }

private:
    std::function<int(CSocketBase*)> m_binder;
};

// Recv 回调：对外仍然暴露 operator()(CSocketBase*, const Buffer&)
template <typename _FUNCTION_, typename... _ARGS_>
class CRecvFunction : public CFunctionBase
{
public:
    CRecvFunction(_FUNCTION_ func, _ARGS_... args)
        : m_binder(std::bind(std::move(func), std::move(args)...))
    {
    }

    int operator()(CSocketBase* pClient, const Buffer& data) override
    {
        // 这里假定 bind 的目标最终能接受 (CSocketBase*, const Buffer&) 参数
        return m_binder(pClient, data);
    }

private:
    std::function<int(CSocketBase*, const Buffer&)> m_binder;
};
