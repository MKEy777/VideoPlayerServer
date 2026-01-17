#ifndef RECORDFILE_H
#define RECORDFILE_H
#include <QString>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonDocument>
#include "ssltool.h"

//配置和信息记录文件类
class RecordFile
{
public:
    RecordFile(const QString& path);
    ~RecordFile();
    QJsonObject& config();
    bool save();
private:
    QJsonObject m_config;
    QString m_path;
    SslTool tool;
};

#endif // RECORDFILE_H
