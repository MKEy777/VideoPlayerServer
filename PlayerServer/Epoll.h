#pragma once
#include <unistd.h>
#include <sys/epoll.h>
#include <vector>
#include <signal.h>
#include <memory.h>
#include <errno.h>

#define EVENT_SIZE 128

//封装epoll_data_t联合体
/*
    typedef union epoll_data
    {
        void* ptr;
        int fd;
        uint32_t u32;
        uint64_t u64;
    } epoll_data_t;
*/
class EpollData {
public:
    EpollData() { m_data.u64 = 0; }
    EpollData(void* ptr) { m_data.ptr = ptr; }
    explicit EpollData(int fd) { m_data.fd = fd; }
    explicit EpollData(uint32_t u32) { m_data.u32 = u32; }
    explicit EpollData(uint64_t u64) { m_data.u64 = u64; }

    EpollData(const EpollData& other) { m_data.u64 = other.m_data.u64; }

public://运算符重载
    EpollData& operator=(const EpollData& other) {
        if (this != &other) {
            m_data.u64 = other.m_data.u64;
        }
        return *this;
    }

    EpollData& operator=(void* ptr) {
        m_data.ptr = ptr;
        return *this;
    }

    EpollData& operator=(int fd) {
        m_data.fd = fd;
        return *this;
    }

    EpollData& operator=(uint32_t u32) {
        m_data.u32 = u32;
        return *this;
    }

    EpollData& operator=(uint64_t u64) {
        m_data.u64 = u64;
        return *this;
    }

    operator epoll_data_t() { return m_data; }
    operator epoll_data_t() const { return m_data; }

    operator epoll_data_t* () { return &m_data; }
    operator const epoll_data_t* () const { return &m_data; }

private:
    epoll_data_t m_data;
};

/*
struct epoll_event
{
    uint32_t events;	// Epoll events 
    epoll_data_t data;	// User data variable 
} __EPOLL_PACKED;
*/

using EPEvents = std::vector<epoll_event>;

class CEpoll {
public:
    CEpoll() : m_epoll(-1) {}
    ~CEpoll() { Close(); }

    CEpoll(const CEpoll&) = delete;
    CEpoll& operator=(const CEpoll&) = delete;

    operator int() const { return m_epoll; }

public:
    int Create(unsigned count) {
        if (m_epoll != -1) return -1;
        m_epoll = epoll_create(static_cast<int>(count));
        if (m_epoll == -1) return -2;
        return 0;
    }

    // 返回值：<0 错误；=0 没有事件；>0 返回事件个数
    ssize_t WaitEvents(EPEvents& events, int timeout = 10) {
        if (m_epoll == -1) return -1;

        EPEvents tmp(EVENT_SIZE);
        int ret = epoll_wait(m_epoll, tmp.data(), static_cast<int>(tmp.size()), timeout);
        if (ret == -1) {
            if (errno == EINTR || errno == EAGAIN) {
                return 0;
            }
            return -2;
        }

        if (ret > static_cast<int>(events.size())) {
            events.resize(ret);
        }
        memcpy(events.data(), tmp.data(), sizeof(epoll_event) * ret);
        return ret;
    }

    int Add(int fd, const EpollData& data = EpollData((void*)0), uint32_t events = EPOLLIN) {
        if (m_epoll == -1) return -1;
        epoll_event ev;
        ev.events = events;
        ev.data = data;
        int ret = epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &ev);
        if (ret == -1) return -2;
        return 0;
    }

    int Modify(int fd, uint32_t events, const EpollData& data = EpollData((void*)0)) {
        if (m_epoll == -1) return -1;
        epoll_event ev;
        ev.events = events;
        ev.data = data;
        int ret = epoll_ctl(m_epoll, EPOLL_CTL_MOD, fd, &ev);
        if (ret == -1) return -2;
        return 0;
    }

    int Del(int fd) {
        if (m_epoll == -1) return -1;
        int ret = epoll_ctl(m_epoll, EPOLL_CTL_DEL, fd, nullptr);
        if (ret == -1) return -2;
        return 0;
    }

    void Close() {
        if (m_epoll != -1) {
            int fd = m_epoll;
            m_epoll = -1;
            ::close(fd);
        }
    }
private:
    int m_epoll;
};