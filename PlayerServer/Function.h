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

template <typename _FUNCTION_, typename... _ARGS_>
class CFunction : public CFunctionBase
{
public:
    // 利用 std::forward 完美转发函数及其参数，并存储在 std::bind 中
    CFunction(_FUNCTION_ func, _ARGS_... args)
        : m_binder(std::bind(std::move(func), std::move(args)...))
    {}

    int operator()() override
    {
        return m_binder();
    }

private:
    // 使用 std::function 存储绑定后的可执行体
    std::function<int()> m_binder;
};

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
        return m_binder(pClient);
    }

private:
    std::function<int(CSocketBase*)> m_binder;
};

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
        return m_binder(pClient, data);
    }

private:
    std::function<int(CSocketBase*, const Buffer&)> m_binder;
};
