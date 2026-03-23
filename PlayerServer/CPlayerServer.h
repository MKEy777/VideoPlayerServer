#pragma once

#include "Logger.h"
#include "CServer.h"
#include <map>
#include <cerrno>
#include <cstring>
#include <functional>
#include "jsoncpp/json.h"
#include "HttpParser.h"
#include "MysqlClient.h"
#include "Crypto.h"
#include <mutex>

DECLARE_TABLE_CLASS(user_mysql, _mysql_table_)
DECLARE_MYSQL_FIELD(TYPE_INT, user_id, NOT_NULL | PRIMARY_KEY | AUTOINCREMENT, "INTEGER", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_qq, NOT_NULL, "VARCHAR", "(15)", "", "")  //QQ号
DECLARE_MYSQL_FIELD(TYPE_VARCHAR, user_phone, DEFAULT, "VARCHAR", "(11)", "'18888888888'", "")  //手机
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_name, NOT_NULL, "TEXT", "", "", "")    //姓名
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_nick, NOT_NULL, "TEXT", "", "", "")    //昵称
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat, DEFAULT, "TEXT", "", "NULL", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_wechat_id, DEFAULT, "TEXT", "", "NULL", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_address, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_province, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_country, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_age, DEFAULT | CHECK, "INTEGER", "", "18", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_male, DEFAULT, "BOOL", "", "1", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_flags, DEFAULT, "TEXT", "", "0", "")
DECLARE_MYSQL_FIELD(TYPE_REAL, user_experience, DEFAULT, "REAL", "", "0.0", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_level, DEFAULT | CHECK, "INTEGER", "", "0", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_class_priority, DEFAULT, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_REAL, user_time_per_viewer, DEFAULT, "REAL", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_career, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_password, NOT_NULL, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_birthday, NONE, "DATETIME", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_describe, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_TEXT, user_education, NONE, "TEXT", "", "", "")
DECLARE_MYSQL_FIELD(TYPE_INT, user_register_time, DEFAULT, "DATETIME", "", "LOCALTIME()", "")
DECLARE_TABLE_CLASS_EDN()

/*
* 1. 客户端的地址问题
* 2. 连接回调的参数问题
* 3. 接收回调的参数问题
*/

#define ERR_RETURN(ret, err) if(ret!=0){TRACEE("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));return err;}

#define WARN_CONTINUE(ret) if(ret!=0){TRACEW("ret= %d errno = %d msg = [%s]", ret, errno, strerror(errno));continue;}


class CPlayerServer : public CBusiness
{
public:
    explicit CPlayerServer(unsigned count)
        : CBusiness(), m_count(count){}

    ~CPlayerServer(){
        if (m_db) {
            CDatabaseClient* db = m_db;
            m_db = NULL;
            db->Close();
            delete db;
        }
        m_epoll.Close();
        m_pool.Close();
        for (auto& it : m_mapClients) {
            if (it.second) {
                delete it.second;
            }
        }
        m_mapClients.clear();
    }

    virtual int BusinessProcess(CProcess* proc) {
        using namespace std::placeholders;
        int ret = 0; 
        m_db = new CMysqlClient();
        if(m_db==NULL){
            TRACEE("no more memory!");
            return -1;
		}
        KeyValue args;
        args["host"] = "127.0.0.1";
        args["user"] = "root";
        args["password"] = "123456";
        args["port"] = "3306";
        args["db"] = "edoyun";
        ret = m_db->Connect(args);
        ERR_RETURN(ret, -2);
        ret = setConnectedCallback(&CPlayerServer::Connected, this, _1);
        ERR_RETURN(ret, -3);
        ret = setRecvCallback(&CPlayerServer::Received, this, _1, _2);
        ERR_RETURN(ret, -4);
        ret = m_epoll.Create(m_count);
        ERR_RETURN(ret, -5);
        ret = m_pool.Start(m_count);
        ERR_RETURN(ret, -6);
        for (unsigned i = 0; i < m_count; i++) {
            ret = m_pool.AddTask(&CPlayerServer::ThreadFunc, this);
            ERR_RETURN(ret, -7);
        }
        int sock = 0;
        sockaddr_in addrin;
        while (m_epoll != -1) {
            ret = proc->RecvSocket(sock, &addrin);
            if (ret < 0 || (sock == 0))break;
            CSocketBase* pClient = new CSocket(sock);
            if (pClient == NULL) {
                close(sock);
                continue;
            }
            ret = pClient->Init(CSockParam(&addrin, SOCK_ISIP));
            if (ret != 0) {
                TRACEW("Init failed ret=%d...", ret);
                delete pClient; 
                continue;
            }
            m_mapClients[sock] = pClient;
            ret = m_epoll.Add(sock, EpollData((void*)pClient), EPOLLIN | EPOLLONESHOT);
            if (ret != 0) {
                TRACEW("Epoll Add failed ret=%d...", ret);
                CloseClient(pClient);
                continue;
            }
            if (m_connectedcallback) {
                (*m_connectedcallback)(pClient);
            }
            
        }
        return 0;
    }

private:
    int Connected(CSocketBase* pClient) {
        //TODO:客户端连接处理 简单打印一下客户端信息
        sockaddr_in* paddr = *pClient;
        TRACEI("client connected addr %s port:%d", inet_ntoa(paddr->sin_addr), paddr->sin_port);
        return 0;
    }
    int Received(CSocketBase* pClient, const Buffer& data) {
        TRACEI("HTTPdata has been received!");
        //TODO:主要业务，在此处理
        //HTTP 解析
        int ret = 0;
        Buffer response = "";
        ret = HttpParser(data);
        TRACEI("HttpParser ret=%d", ret);
        //验证结果的反馈
        if (ret != 0) {//验证失败
            TRACEE("http parser failed!%d", ret);
        }
        response = MakeResponse(ret);
        ret = pClient->Send(response);
        if (ret != 0) {
            TRACEE("http response failed!%d [%s]", ret, (char*)response);
        }
        else {
            TRACEI("http response success!%d", ret);
        }
        return 0;
    }
    int HttpParser(const Buffer& data) {
        CHttpParser parser;
        size_t size = parser.Parser(data);
        if (size == 0 || (parser.Errno() != 0)) {
            TRACEE("size %llu errno:%u", size, parser.Errno());
            return -1;
        }
        if (parser.Method() == HTTP_GET) {
            //get 处理
            UrlParser url("https://192.168.1.100" + parser.Url());
            int ret = url.Parser();
            if (ret != 0) {
                TRACEE("ret = %d url[%s]", ret, "https://192.168.1.100" + parser.Url());
                return -2;
            }
            Buffer uri = url.Uri();
            TRACEI("**** uri = %s", (char*)uri);
            if (uri == "login") {
                //处理登录
                Buffer time = url["time"];
                Buffer salt = url["salt"];
                Buffer user = url["user"];
                Buffer sign = url["sign"];
                TRACEI("time=%s salt=%s user=%s sign=%s", (char*)time, (char*)salt, (char*)user, (char*)sign);
                //数据库的查询
                user_mysql dbuser;
                Result result;
                Buffer sql = dbuser.Query("user_name=\"" + user + "\"");
                Buffer pwd;
                {
                    std::lock_guard<std::mutex> lock(m_dbMutex);
                    int ret = m_db->Exec(sql, result, dbuser);
                    if (ret != 0) {
                        TRACEE("sql=%s ret=%d", (char*)sql, ret);
                        return -3;
                    }
                    if (result.size() == 0) {
                        TRACEE("no result sql=%s ret=%d", (char*)sql, ret);
                        return -4;
                    }
                    if (result.size() != 1) {
                        TRACEE("more than one sql=%s ret=%d", (char*)sql, ret);
                        return -5;
                    }

                    auto user1 = result.front();
                    //for (auto& kv : user1->Fields) {
                    //    TRACEI("Field key: [%s]", (char*)kv.first);
                    //}
                    auto it = user1->Fields.find("user_password");
                    if (it == user1->Fields.end() || it->second->Value.String == nullptr) {
                        TRACEE("password field missing or null");
                        return -7;
                    }
                    pwd = *it->second->Value.String;
                }
                TRACEI("password = %s", (char*)pwd);
                //登录请求的验证
                const char* MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl";
                Buffer md5str = time + MD5_KEY + pwd + salt;
                TRACEI("md5str = %s", (char*)md5str);
                Buffer md5 = Crypto::MD5(md5str);
                TRACEI("md5 = %s", (char*)md5);
                if (md5 == sign) {
                    return 0;
                }
                return -6;
            }
        }
        else if (parser.Method() == HTTP_POST) {
            //post 处理
        }
        return -7;
    }

    /*
    将业务层处理的结果（错误码 ret），按照 HTTP/1.1 协议标准 和 JSON 数据格式，组装成可以直接通过 Socket 发送出去的完整网络字节流（报文）
    */
    Buffer MakeResponse(int ret) {
        Json::Value root;
        root["status"] = ret;
        if (ret != 0) {
            root["message"] = "Login failed, the username or password may be incorrect！";
        }
        else {
            root["message"] = "success";
        }
        Buffer json = root.toStyledString();
        Buffer result = "HTTP/1.1 200 OK\r\n";
        time_t t;
        time(&t);
        tm* ptm = localtime(&t);
        char temp[64] = "";
        // Wed, 21 Oct 2015 07:28:00 GMT
        strftime(temp, sizeof(temp), "%a, %d %b %G %T GMT\r\n", ptm);
        //strftime(temp, sizeof(temp), "%a, %d %b %Y %H:%M:%S GMT\r\n", ptm);
        Buffer Date = Buffer("Date: ") + temp;
        Buffer Server = "Server: Edoyun/1.0\r\nContent-Type: application/json; charset=utf-8\r\nX-Frame-Options: DENY\r\n";
        snprintf(temp, sizeof(temp), "%d", json.size());
        Buffer Length = Buffer("Content-Length: ") + temp + "\r\n";
        Buffer Stub = "X-Content-Type-Options: nosniff\r\nReferrer-Policy: same-origin\r\n\r\n";
        result += Date + Server + Length + Stub + json;
        TRACEI("response: %s", (char*)result);
        return result;
    }
    void CloseClient(CSocketBase* pClient) {

        if (!pClient) return;
        int fd = (int)(*pClient);
        m_epoll.Del(*pClient); // 先从 epoll 移除
        auto it = m_mapClients.find(fd);

        if (it != m_mapClients.end()) {

            m_mapClients.erase(it);

        }
        delete pClient; // 最后释放内存
    }
private:
    int ThreadFunc()
    {
        EPEvents events;
        while (m_epoll != -1) {
            ssize_t size = m_epoll.WaitEvents(events);
            if (size < 0) break;

            for (ssize_t i = 0; i < size; i++) {
                CSocketBase* pClient = (CSocketBase*)events[i].data.ptr;
                if (!pClient) continue;

                if (events[i].events & EPOLLERR) {
                    TRACEE("EPOLLERR detected on %p", pClient);
                    CloseClient(pClient);
                    continue;
                }

                if (events[i].events & EPOLLIN) {
                    Buffer data(4096);
                    int ret = pClient->Recv(data);

                    if (ret > 0) {
                        if (m_recvcallback) {
                            (*m_recvcallback)(pClient, data);
                        }
                        m_epoll.Modify((int)(*pClient), EPOLLIN | EPOLLONESHOT, EpollData((void*)pClient));
                    }
                    else if (ret == -3) {
                        TRACEI("Client disconnected ptr=%p", pClient);
                        CloseClient(pClient);
                    }
                    else {
                        TRACEE("Recv Failed. ret=%d errno=%d msg=%s", ret, errno, strerror(errno));
                        CloseClient(pClient);
                    }
                }
            }
        }
        return 0;
    }

private:
    CEpoll m_epoll;
    std::map<int, CSocketBase*> m_mapClients;
    CThreadPool m_pool;
    unsigned m_count = 0;
    CDatabaseClient* m_db;
    std::mutex m_dbMutex;
};
