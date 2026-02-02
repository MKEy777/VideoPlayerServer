#include "HttpParser.h"
#include <cstring>
#include <cstdlib>

CHttpParser::CHttpParser()
{
    m_complete = false;

    memset(&m_parser, 0, sizeof(m_parser));
    m_parser.data = this;
    http_parser_init(&m_parser, HTTP_REQUEST);

    memset(&m_settings, 0, sizeof(m_settings));
    m_settings.on_message_begin = &CHttpParser::OnMessageBegin;
    m_settings.on_url = &CHttpParser::OnUrl;
    m_settings.on_status = &CHttpParser::OnStatus;
    m_settings.on_header_field = &CHttpParser::OnHeaderField;
    m_settings.on_header_value = &CHttpParser::OnHeaderValue;
    m_settings.on_headers_complete = &CHttpParser::OnHeadersComplete;
    m_settings.on_body = &CHttpParser::OnBody;
    m_settings.on_message_complete = &CHttpParser::OnMessageComplete;
}

CHttpParser::~CHttpParser()
{
}

CHttpParser::CHttpParser(const CHttpParser& http)
{
    memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
    m_parser.data = this;

    memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
    m_status = http.m_status;
    m_url = http.m_url;
    m_body = http.m_body;
    m_complete = http.m_complete;
    m_lastField = http.m_lastField;
}

CHttpParser& CHttpParser::operator=(const CHttpParser& http)
{
    if (this != &http)
    {
        memcpy(&m_parser, &http.m_parser, sizeof(m_parser));
        m_parser.data = this;

        memcpy(&m_settings, &http.m_settings, sizeof(m_settings));
        m_status = http.m_status;
        m_url = http.m_url;
        m_body = http.m_body;
        m_complete = http.m_complete;
        m_lastField = http.m_lastField;
    }
    return *this;
}

size_t CHttpParser::Parser(const Buffer& data)
{
    m_complete = false;

    size_t ret = http_parser_execute(
        &m_parser,
        &m_settings,
        data,
        data.size()
    );

    if (!m_complete)
    {
        m_parser.http_errno = 0x7F;
        return 0;
    }
    return ret;
}

// ---------- static callbacks ----------

int CHttpParser::OnMessageBegin(http_parser* parser)
{
    return static_cast<CHttpParser*>(parser->data)->OnMessageBegin();
}

int CHttpParser::OnUrl(http_parser* parser, const char* at, size_t length)
{
    return static_cast<CHttpParser*>(parser->data)->OnUrl(at, length);
}

int CHttpParser::OnStatus(http_parser* parser, const char* at, size_t length)
{
    return static_cast<CHttpParser*>(parser->data)->OnStatus(at, length);
}

int CHttpParser::OnHeaderField(http_parser* parser, const char* at, size_t length)
{
    return static_cast<CHttpParser*>(parser->data)->OnHeaderField(at, length);
}

int CHttpParser::OnHeaderValue(http_parser* parser, const char* at, size_t length)
{
    return static_cast<CHttpParser*>(parser->data)->OnHeaderValue(at, length);
}

int CHttpParser::OnHeadersComplete(http_parser* parser)
{
    return static_cast<CHttpParser*>(parser->data)->OnHeadersComplete();
}

int CHttpParser::OnBody(http_parser* parser, const char* at, size_t length)
{
    return static_cast<CHttpParser*>(parser->data)->OnBody(at, length);
}

int CHttpParser::OnMessageComplete(http_parser* parser)
{
    return static_cast<CHttpParser*>(parser->data)->OnMessageComplete();
}

// ---------- member callbacks ----------

int CHttpParser::OnMessageBegin()
{
    return 0;
}

int CHttpParser::OnUrl(const char* at, size_t length)
{
    m_url = Buffer(at, length);
    return 0;
}

int CHttpParser::OnStatus(const char* at, size_t length)
{
    m_status = Buffer(at, length);
    return 0;
}

int CHttpParser::OnHeaderField(const char* at, size_t length)
{
    m_lastField = Buffer(at, length);
    return 0;
}

int CHttpParser::OnHeaderValue(const char* at, size_t length)
{
    m_HeaderValues[m_lastField] = Buffer(at, length);
    return 0;
}

int CHttpParser::OnHeadersComplete()
{
    return 0;
}

int CHttpParser::OnBody(const char* at, size_t length)
{
    m_body = Buffer(at, length);
    return 0;
}

int CHttpParser::OnMessageComplete()
{
    m_complete = true;
    return 0;
}

// ================= UrlParser =================

UrlParser::UrlParser(const Buffer& url)
{
    m_url = url;
    m_port = 80;
}

int UrlParser::Parser()
{
    // 协议
    const char* pos = m_url;
    const char* target = strstr(pos, "://");
    if (!target) return -1;

    m_protocol = Buffer(pos, target);

    // 主机和端口
    pos = target + 3;
    target = strchr(pos, '/');
    if (!target)
    {
        m_host = pos;
        return 0;
    }

    Buffer value(pos, target);
    if (value.size() == 0) return -2;

    target = strchr(value, ':');
    if (target)
    {
        m_host = Buffer(value, target);
        m_port = atoi(target + 1);
    }
    else
    {
        m_host = value;
    }

    // URI
    pos = strchr(pos, '/');
    target = strchr(pos, '?');
    if (!target)
    {
        m_uri = pos;
        return 0;
    }

    m_uri = Buffer(pos, target);

    // 参数
    pos = target + 1;
    while (pos)
    {
        const char* next = strchr(pos, '&');
        Buffer kv = next ? Buffer(pos, next) : Buffer(pos);
        const char* eq = strchr(kv, '=');
        if (!eq) return -3;

        m_values[Buffer(kv, eq)] = Buffer(eq + 1, kv + kv.size());
        if (!next) break;
        pos = next + 1;
    }

    return 0;
}

Buffer UrlParser::operator[](const Buffer& name) const
{
    auto it = m_values.find(name);
    if (it == m_values.end()) return Buffer();
    return it->second;
}

void UrlParser::SetUrl(const Buffer& url)
{
    m_url = url;
    m_protocol = "";
    m_host = "";
    m_uri = "";
    m_port = 80;
    m_values.clear();
}
