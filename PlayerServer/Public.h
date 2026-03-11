#pragma once
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>

/**
 * @brief 通用字节缓冲区类
 * 设计目标：提供类似 std::string 的操作体验，兼容 C 风格 API
 * 特点：自动管理内存，强制末尾 '\0' 填充，支持 const 隐式转 char*
 */
class Buffer {
public:
    // ===== 构造与析构 =====
    Buffer() { ensure_zterm(); }

    // 预分配指定容量并设定长度
    explicit Buffer(size_t capacity) {
        reserve(capacity);
        len_ = capacity;
        ensure_zterm();
    }

    // 从 C 字符串构造
    Buffer(const char* cstr) { assign_cstr(cstr); }
    // 从 std::string 构造
    Buffer(const std::string& s) { assign_cstr(s.c_str()); }

    // 从指定内存地址和长度构造
    Buffer(const char* data, size_t length) {
        if (!data || length == 0) { clear(); return; }
        buf_.assign(data, data + length);
        len_ = length;
        ensure_zterm();
    }

    // 从指针区间构造 [begin, end)
    Buffer(const char* begin, const char* end) {
        if (!begin || !end || end <= begin) { clear(); return; }
        size_t length = static_cast<size_t>(end - begin);
        buf_.assign(begin, end);
        len_ = length;
        ensure_zterm();
    }

    // ===== 迭代器支持 (支持范围 for 循环: for(auto c : buffer)) =====
    char* begin() noexcept { return data(); }
    char* end() noexcept { return data() + len_; }
    const char* begin() const noexcept { return data(); }
    const char* end() const noexcept { return data() + len_; }

    // ===== 基础属性获取 =====
    size_t size() const noexcept { return len_; }      // 返回有效数据长度
    bool empty() const noexcept { return len_ == 0; }
    size_t capacity() const noexcept {                 // 返回实际可用容量
        return buf_.empty() ? 0 : (buf_.size() - 1);
    }

    // ===== 内存管理接口 =====
    // 预留内存空间
    void reserve(size_t new_capacity) {
        if (buf_.size() < new_capacity + 1) buf_.resize(new_capacity + 1);
        ensure_zterm();
    }

    // 重新设定数据有效长度
    void resize(size_t new_size) {
        len_ = new_size;
        if (buf_.size() < len_ + 1) buf_.resize(len_ + 1);
        buf_[len_] = '\0';
    }

    // 清空内容，保留最小容量
    void clear() noexcept {
        len_ = 0;
        if (buf_.empty()) buf_.resize(1);
        buf_[0] = '\0';
    }

    // ===== 数据访问与类型转换 =====
    char* data() noexcept { return buf_.data(); }
    const char* data() const noexcept { return buf_.data(); }
    operator char* () const noexcept { return const_cast<char*>(buf_.data()); }
    operator unsigned char* () noexcept { return reinterpret_cast<unsigned char*>(buf_.data());}
    operator const unsigned char* () const noexcept { return reinterpret_cast<const unsigned char*>(buf_.data());}

    // 获取当前有效数据末尾的可写指针（用于 Recv 等直接写入场景）
    char* writable_tail(size_t need) {
        reserve(len_ + need);
        return buf_.data() + len_;
    }

    // 确保以 \0 结尾并返回 C 风格常量字符串
    const char* c_str() const noexcept {
        const_cast<Buffer*>(this)->ensure_zterm();
        return buf_.data();
    }

    // 转换为 std::string
    std::string to_string() const {
        return std::string(data(), data() + len_);
    }

    // ===== 数据更新与追加 =====
    void assign_cstr(const char* cstr) {
        if (!cstr) { clear(); return; }
        size_t length = std::strlen(cstr);
        buf_.assign(cstr, cstr + length);
        len_ = length;
        ensure_zterm();
    }

    void append(const char* data, size_t length) {
        if (!data || length == 0) return;
        reserve(len_ + length);
        std::memcpy(buf_.data() + len_, data, length);
        len_ += length;
        ensure_zterm();
    }

    void append(const char* cstr) { if (cstr) append(cstr, std::strlen(cstr)); }
    void append(const std::string& s) { append(s.data(), s.size()); }
    void append(char c) {
        reserve(len_ + 1);
        buf_[len_] = c;
        ++len_;
        buf_[len_] = '\0';
    }

    // 运算符重载：支持连续追加
    Buffer& operator+=(const char* cstr) { append(cstr); return *this; }
    Buffer& operator+=(const std::string& s) { append(s); return *this; }
    Buffer& operator+=(char c) { append(c); return *this; }
    Buffer& operator+=(const Buffer& rhs) { append(rhs.data(), rhs.size()); return *this; }

    Buffer& operator=(const char* cstr) { assign_cstr(cstr); return *this; }
    Buffer& operator=(const std::string& s) { assign_cstr(s.c_str()); return *this; }

    bool operator==(const char* cstr) const {
        if (cstr == nullptr) return empty();
        // 长度不等直接返回 false，提高效率
        size_t cstr_len = std::strlen(cstr);
        if (len_ != cstr_len) return false;
        return std::memcmp(data(), cstr, len_) == 0;
    }

    bool operator!=(const char* cstr) const {
        return !(*this == cstr);
    }

    bool operator==(const std::string& s) const {return (len_ == s.size()) && (std::memcmp(data(), s.data(), len_) == 0);}
    bool operator!=(const std::string& s) const { return !(*this == s); }

    bool operator==(const Buffer& rhs) const { return (len_ == rhs.len_) && (std::memcmp(data(), rhs.data(), len_) == 0);}
    bool operator!=(const Buffer& rhs) const { return !(*this == rhs); }

    // ===== 运算符重载：拼接操作 =====

    // Buffer + Buffer
    Buffer operator+(const Buffer& rhs) const {
        Buffer res = *this;
        res.append(rhs.data(), rhs.size());
        return res;
    }

    // Buffer + const char* (解决 "SELECT " + name 的情况)
    Buffer operator+(const char* rhs) const {
        Buffer res = *this;
        res.append(rhs);
        return res;
    }

    // Buffer + char (解决 sql + ')' 的情况)
    Buffer operator+(char rhs) const {
        Buffer res = *this;
        res.append(rhs);
        return res;
    }

    // 友元：解决左操作数不是 Buffer 的拼接（如 '"' + Name）
    friend Buffer operator+(const char* lhs, const Buffer& rhs) {
        Buffer res;
        if (lhs) {
            size_t l = std::strlen(lhs);
            res.reserve(l + rhs.size());
            res.append(lhs, l);
        }
        res.append(rhs.data(), rhs.size());
        return res;
    }

    friend Buffer operator+(char lhs, const Buffer& rhs) {
        Buffer res;
        res.reserve(1 + rhs.size());
        res.append(lhs);
        res.append(rhs.data(), rhs.size());
        return res;
    }

    // 提供 map/set 的 Key 比较支持
    bool operator<(const Buffer& rhs) const {
        const size_t n = std::min(len_, rhs.len_);
        int c = std::memcmp(data(), rhs.data(), n);
        if (c != 0) return c < 0;
        return len_ < rhs.len_;
    }

private:
    // 内部函数：确保缓冲区始终以零结尾，满足 C 字符串约定
    void ensure_zterm() {
        if (buf_.empty()) buf_.resize(1);
        if (buf_.size() < len_ + 1) buf_.resize(len_ + 1);
        buf_[len_] = '\0';
    }

private:
    std::vector<char> buf_; // 连续内存存储容器
    size_t len_{ 0 };       // 有效载荷长度
};