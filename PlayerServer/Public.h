#pragma once
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>

class Buffer {
public:
    Buffer() { ensure_zterm(); }

    explicit Buffer(size_t capacity) {
        reserve(capacity);
        len_ = capacity;
        ensure_zterm();
    }

    Buffer(const char* cstr) { assign_cstr(cstr); }
    Buffer(const std::string& s) { assign_cstr(s.c_str()); }

    // 从 data + length 构造
    Buffer(const char* data, size_t length) {
        if (!data || length == 0) { clear(); return; }
        buf_.assign(data, data + length);
        len_ = length;
        ensure_zterm();
    }

    // 从 [begin, end) 构造
    Buffer(const char* begin, const char* end) {
        if (!begin || !end || end <= begin) { clear(); return; }
        size_t length = static_cast<size_t>(end - begin);
        buf_.assign(begin, end);
        len_ = length;
        ensure_zterm();
    }

    // ===== 基本属性 =====
    size_t size() const noexcept { return len_; }   // 有效字节数
    bool empty() const noexcept { return len_ == 0; }

    // 可用容量（不含末尾 '\0'）
    size_t capacity() const noexcept {
        return buf_.empty() ? 0 : (buf_.size() - 1);
    }

    // ===== 内存管理 =====
    void reserve(size_t new_capacity) {
        if (buf_.size() < new_capacity + 1) buf_.resize(new_capacity + 1);
        ensure_zterm();
    }

    // 设定有效长度（常用于 Recv 后：resize(r)）
    void resize(size_t new_size) {
        len_ = new_size;
        if (buf_.size() < len_ + 1) buf_.resize(len_ + 1);
        buf_[len_] = '\0';
    }

    void clear() noexcept {
        len_ = 0;
        if (buf_.empty()) buf_.resize(1);
        buf_[0] = '\0';
    }

    // ===== 数据访问 =====
    char* data() noexcept { return buf_.data(); }
    const char* data() const noexcept { return buf_.data(); }

    // 为追加写入预留空间，并返回可写指针（从当前 len_ 起）
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

    std::string to_string() const {
        return std::string(data(), data() + len_);
    }

    // ===== 赋值 =====
    void assign_cstr(const char* cstr) {
        if (!cstr) { clear(); return; }
        size_t length = std::strlen(cstr);
        buf_.assign(cstr, cstr + length);
        len_ = length;
        ensure_zterm();
    }

    // ===== 追加 / 拼接 =====
    void append(const char* data, size_t length) {
        if (!data || length == 0) return;
        reserve(len_ + length);
        std::memcpy(buf_.data() + len_, data, length);
        len_ += length;
        ensure_zterm();
    }

    void append(const char* cstr) {
        if (!cstr) return;
        append(cstr, std::strlen(cstr));
    }

    void append(const std::string& s) {
        append(s.data(), s.size());
    }

    void append(char c) {
        reserve(len_ + 1);
        buf_[len_] = c;
        ++len_;
        buf_[len_] = '\0';
    }

    Buffer& operator+=(const char* cstr) { append(cstr); return *this; }
    Buffer& operator+=(const std::string& s) { append(s); return *this; }
    Buffer& operator+=(char c) { append(c); return *this; }

    Buffer& operator=(const char* cstr) {
        assign_cstr(cstr);
        return *this;
    }

    Buffer& operator=(const std::string& s) {
        assign_cstr(s.c_str());
        return *this;
    }

    // ===== 用作 map key 的字典序比较 =====
    bool operator<(const Buffer& rhs) const {
        const size_t n = std::min(len_, rhs.len_);
        int c = std::memcmp(data(), rhs.data(), n);
        if (c != 0) return c < 0;
        return len_ < rhs.len_;
    }

private:
    void ensure_zterm() {
        if (buf_.empty()) buf_.resize(1);
        if (buf_.size() < len_ + 1) buf_.resize(len_ + 1);
        buf_[len_] = '\0';
    }

private:
    std::vector<char> buf_;
    size_t len_{ 0 };
};