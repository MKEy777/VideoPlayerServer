#pragma once
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <fcntl.h>
#include <vector>
#include <cstring>
//#include <algorithm>

class Buffer {
public:
    Buffer() { ensure_zterm(); }

    // 预留容量 n（不是有效长度）
    explicit Buffer(size_t n) {
        reserve(n);
        len_ = 0;
        ensure_zterm();
    }

    Buffer(const char* s) { assign_cstr(s); }
    Buffer(const std::string& s) { assign_cstr(s.c_str()); }

    size_t size() const noexcept { return len_; }          // 有效字节数
    bool empty() const noexcept { return len_ == 0; }

    size_t capacity() const noexcept {
        return buf_.empty() ? 0 : (buf_.size() - 1);       // 可用容量（不含 '\0'）
    }

    void reserve(size_t n) {
        if (buf_.size() < n + 1) buf_.resize(n + 1);
        ensure_zterm();
    }

    // 设定有效长度（常用于 Recv 后：resize(r)）
    void resize(size_t n) {
        len_ = n;
        if (buf_.size() < len_ + 1) buf_.resize(len_ + 1);
        buf_[len_] = '\0';
    }

    void clear() noexcept {
        len_ = 0;
        if (buf_.empty()) buf_.resize(1);
        buf_[0] = '\0';
    }

    // 用于“写入”的指针（Socket Recv 典型会用这个）
    char* data() noexcept { return buf_.data(); }
    const char* data() const noexcept { return buf_.data(); }

    // 额外提供：从 len_ 后开始写
    char* writable_tail(size_t need) {
        reserve(len_ + need);
        return buf_.data() + len_;
    }

    operator char* () noexcept { return data(); }
    operator const char* () const noexcept { return data(); }

    const char* c_str() const noexcept {
        const_cast<Buffer*>(this)->ensure_zterm();
        return buf_.data();
    }

    void assign_cstr(const char* s) {
        if (!s) { clear(); return; }
        size_t n = std::strlen(s);
        buf_.assign(s, s + n);
        len_ = n;
        ensure_zterm();
    }

    std::string to_string() const {
        return std::string(data(), data() + len_);
    }

    // ====== 追加 / 拼接（适配 LogInfo）======
    void append(const char* s, size_t n) {
        if (!s || n == 0) return;
        reserve(len_ + n);
        std::memcpy(buf_.data() + len_, s, n);
        len_ += n;
        ensure_zterm();
    }
    void append(char c) {
        reserve(len_ + 1);
        buf_[len_] = c;
        len_++;
        buf_[len_] = '\0';
    }

    void append(const char* s) {
        if (!s) return;
        append(s, std::strlen(s));
    }

    void append(const std::string& s) {
        append(s.data(), s.size());
    }

    Buffer& operator+=(const char* s) { append(s); return *this; }
    Buffer& operator+=(const std::string& s) { append(s); return *this; }
    Buffer& operator+=(char c) {append(c);return *this;}

private:
    void ensure_zterm() {
        if (buf_.empty()) buf_.resize(1);
        if (buf_.size() < len_ + 1) buf_.resize(len_ + 1);
        buf_[len_] = '\0';
    }

private:
    std::vector<char> buf_;
    size_t len_{ 0 }; // 逻辑长度（有效字节数）
};

enum SockAttr {
    SOCK_ISSERVER = 1, // 是否服务器：1=服务器，0=客户端
    SOCK_ISNONBLOCK = 2, // 是否非阻塞：1=非阻塞，0=阻塞
    SOCK_ISUDP = 4, // 是否 UDP：1=UDP，0=TCP
};

//套接字参数封装类
class CSockParam {
public:
    CSockParam() : port(-1), attr(0) {
        std::memset(&addr_in, 0, sizeof(addr_in));
        std::memset(&addr_un, 0, sizeof(addr_un));
    }

    // 网络套接字 (IPv4)
    CSockParam(const Buffer& ip, short port, int attr)
        : ip(ip), port(port), attr(attr) {
        std::memset(&addr_in, 0, sizeof(addr_in));
        std::memset(&addr_un, 0, sizeof(addr_un));

        addr_in.sin_family = AF_INET;
        addr_in.sin_port = htons(static_cast<uint16_t>(port));

        // 推荐 inet_pton
        if (::inet_pton(AF_INET, ip.c_str(), &addr_in.sin_addr) != 1) {
            // 解析失败：你可以选择置 0 或留给上层处理
            addr_in.sin_addr.s_addr = INADDR_NONE;
        }
    }

    // 本地套接字 (Unix Domain Socket)
    CSockParam(const Buffer& path, int attr)
        : ip(path), port(-1), attr(attr) {
        std::memset(&addr_in, 0, sizeof(addr_in));
        std::memset(&addr_un, 0, sizeof(addr_un));

        addr_un.sun_family = AF_UNIX;
        // 安全拷贝，防止路径过长溢出
        std::strncpy(addr_un.sun_path, path.c_str(), sizeof(addr_un.sun_path) - 1);
        addr_un.sun_path[sizeof(addr_un.sun_path) - 1] = '\0';
    }

    sockaddr* addrin() { return reinterpret_cast<sockaddr*>(&addr_in); }
    sockaddr* addrun() { return reinterpret_cast<sockaddr*>(&addr_un); }
    const sockaddr* addrin() const { return reinterpret_cast<const sockaddr*>(&addr_in); }
    const sockaddr* addrun() const { return reinterpret_cast<const sockaddr*>(&addr_un); }

public:
    sockaddr_in addr_in{};
    sockaddr_un addr_un{};

    Buffer ip;   // ip 或 unix path（内部保证 '\0'）
    short  port;
    int    attr;
};

class CSocketBase {
public:
    CSocketBase() {
        m_socket = -1;
        m_status = 0; // 初始化未完成
    }

    virtual ~CSocketBase() { Close(); }
    operator int() const noexcept { return m_socket; }
public:
    // 初始化：服务器创建/bind/listen；客户端创建
    virtual int Init(const CSockParam& param) = 0;
    // 连接：服务器 accept；客户端 connect；udp 可忽略
    virtual int Link(CSocketBase** pClient = NULL) = 0;
    // 发送数据
    virtual int Send(const Buffer& data) = 0;
    // 接收数据
    virtual int Recv(Buffer& data) = 0;
    // 关闭连接
    virtual int Close() {
        m_status = 3;
        if (m_socket != -1) {
            int fd = m_socket;
            m_socket = -1;
            close(fd);
        }
        return 0;
    }

protected:
    int m_socket;   // 套接字描述符
    int m_status;   // 0:未完成 1:初始化完成 2:连接完成 3:已关闭
    CSockParam m_param; // 初始化参数
};

class CLocalSocket : public CSocketBase {
public:
    CLocalSocket() : CSocketBase() {}
    CLocalSocket(int sock) : CSocketBase() { m_socket = sock; }
    virtual ~CLocalSocket() { Close(); } // 析构自动关闭
   
public:
    virtual int Init(const CSockParam& param) {
        if (m_status != 0) return -1; // 已初始化过

        m_param = param;
        int type = (m_param.attr & SOCK_ISUDP) ? SOCK_DGRAM : SOCK_STREAM; // DGRAM/STREAM

        if (m_socket == -1)
            m_socket = socket(PF_LOCAL, type, 0); // 创建Unix域socket
        if (m_socket == -1) return -2;

        int ret = 0;
        if (m_param.attr & SOCK_ISSERVER) { // 服务器：bind + listen
            ret = bind(m_socket, m_param.addrun(), sizeof(sockaddr_un));
            if (ret == -1) return -3;
            ret = listen(m_socket, 32); // 监听队列
            if (ret == -1) return -4;
        }

        if (m_param.attr & SOCK_ISNONBLOCK) { // 设置非阻塞
            int option = fcntl(m_socket, F_GETFL);
            if (option == -1) return -5;

            option |= O_NONBLOCK;
            ret = fcntl(m_socket, F_SETFL, option);
            if (ret == -1) return -6;
        }

        m_status = 1; // 初始化完成
        return 0;
    }

    virtual int Link(CSocketBase** pClient = NULL) {// 传入“指针的地址”
        if (m_status <= 0 || (m_socket == -1)) return -1; // 未初始化/无效fd

        int ret = 0;
        if (m_param.attr & SOCK_ISSERVER) {
            if (pClient == NULL) return -2; // 需要返回新客户端对象

            CSockParam param;               // 接收对端地址（Unix下通常没啥用）
            socklen_t len = sizeof(sockaddr_un);

            int fd = accept(m_socket, param.addrun(), &len); // 接受连接
            if (fd == -1) return -3;

            *pClient = new CLocalSocket(fd); // 用accept得到的fd构造
            if (*pClient == NULL) return -4;

            ret = (*pClient)->Init(param);   // 初始化客户端socket对象（复用Init逻辑）
            if (ret != 0) {
                delete (*pClient);
                *pClient = NULL;
                return -5;
            }
        }
        else { // 客户端：connect
            ret = connect(m_socket, m_param.addrun(), sizeof(sockaddr_un));
            if (ret != 0) return -6;
        }

        m_status = 2; // 连接完成
        return 0;
    }

    virtual int Send(const Buffer& data) {
        if (m_status < 2 || (m_socket == -1)) return -1; // 未连接/无效fd

        ssize_t index = 0;
        while (index < (ssize_t)data.size()) { // 循环发送直到发完
            ssize_t len = write(m_socket, (const char*)data + index, data.size() - index);
            if (len == 0) return -2; // 一般不会出现：写0字节
            if (len < 0) {           // 错误处理
                if (errno == EINTR) continue; // 被信号打断：重试
                if (errno == EAGAIN || errno == EWOULDBLOCK) return 0; // 非阻塞：暂时不可写
                return -3; // 其他错误
            }
            index += len;
        }
        return 0;
    }

    // >0 收到字节数；0 没有数据但无错；<0 错误/断开
    virtual int Recv(Buffer& data) {
        if (m_status < 2 || (m_socket == -1)) return -1; // 未连接/无效fd

        ssize_t len = read(m_socket, data, data.size()); // 读入预分配缓冲(data.size())
        if (len > 0) {
            data.resize(len); // 收缩为实际长度
            return (int)len;
        }
        if (len < 0) {
            // EINTR：被信号打断；EAGAIN/EWOULDBLOCK：非阻塞下暂时无数据
            if (errno == EINTR) return 0;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            return -2; // 其他读错误
        }
        return -3; // len==0：对端关闭
    }

    virtual int Close() {
        return CSocketBase::Close(); // 关闭fd并标记状态
    }
};