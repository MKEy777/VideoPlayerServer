#pragma once

#include "Socket.h"
#include "http_parser.h"
#include <map>

// HTTP 请求解析器：基于 http-parser（回调驱动）
// 作用：从原始字节流中解析出 Method / Url / Headers / Body 等信息
class CHttpParser
{
private:
    http_parser m_parser;                // http-parser 内部状态机
    http_parser_settings m_settings;     // 回调函数表

    std::map<Buffer, Buffer> m_HeaderValues; // 请求头：Field -> Value
    Buffer m_status;                     // 状态行（通常用于 HTTP_RESPONSE，解析请求时一般为空）
    Buffer m_url;                        // 请求 URL（path?query）
    Buffer m_body;                       // 请求 Body（注意：当前实现是覆盖式保存，非累加）
    Buffer m_lastField;                  // 临时保存最近一次 Header Field（用于和 Value 配对）
    bool m_complete;                     // 是否解析到 message_complete

public:
    CHttpParser();
    ~CHttpParser();

    CHttpParser(const CHttpParser& http);
    CHttpParser& operator=(const CHttpParser& http);

public:
    // 解析输入数据；返回已消费的字节数
    // 若未解析完整（m_complete=false），当前实现返回 0 并设置 errno=0x7F
    size_t Parser(const Buffer& data);

    // HTTP 方法（GET/POST/...）参考 http_parser.h 的 HTTP_METHOD_MAP
    unsigned Method() const { return m_parser.method; }

    // 解析结果访问接口
    const std::map<Buffer, Buffer>& Headers() const { return m_HeaderValues; }
    const Buffer& Status() const { return m_status; }
    const Buffer& Url() const { return m_url; }
    const Buffer& Body() const { return m_body; }
    unsigned Errno() const { return m_parser.http_errno; } // http-parser 错误码

protected:
    // http-parser 静态回调：负责把 parser->data 转回对象实例并转发到成员函数
    static int OnMessageBegin(http_parser* parser);
    static int OnUrl(http_parser* parser, const char* at, size_t length);
    static int OnStatus(http_parser* parser, const char* at, size_t length);
    static int OnHeaderField(http_parser* parser, const char* at, size_t length);
    static int OnHeaderValue(http_parser* parser, const char* at, size_t length);
    static int OnHeadersComplete(http_parser* parser);
    static int OnBody(http_parser* parser, const char* at, size_t length);
    static int OnMessageComplete(http_parser* parser);

    // 成员回调：实际处理解析到的内容（当前实现只保存最终结果）
    int OnMessageBegin();
    int OnUrl(const char* at, size_t length);
    int OnStatus(const char* at, size_t length);
    int OnHeaderField(const char* at, size_t length);
    int OnHeaderValue(const char* at, size_t length);
    int OnHeadersComplete();
    int OnBody(const char* at, size_t length);
    int OnMessageComplete();
};


// URL 解析器：解析形如 protocol://host[:port]/uri?key=value&...
// 作用：拆出协议、主机、端口、uri 以及 query 参数表
class UrlParser
{
public:
    UrlParser(const Buffer& url);
    ~UrlParser() {}

    // 解析 m_url 并填充各字段；成功返回 0，失败返回负值
    int Parser();

    // 获取 query 参数值：url["name"] -> value（不存在返回空 Buffer）
    Buffer operator[](const Buffer& name) const;

    // 基础字段
    Buffer Protocol() const { return m_protocol; }
    Buffer Host() const { return m_host; }
    int Port() const { return m_port; }   // 默认返回 80

    // 重新设置 URL，并清空上次解析结果
    void SetUrl(const Buffer& url);

private:
    Buffer m_url;                         // 原始 URL
    Buffer m_protocol;                    // 协议（http/https/...）
    Buffer m_host;                        // 域名或 IP
    Buffer m_uri;                         // 路径部分（不含 query）
    int m_port;                           // 端口（默认 80）
    std::map<Buffer, Buffer> m_values;    // query 参数表
};
