#include "loginform.h"
#include "ui_loginform.h"
#include "widget.h"
#include <time.h>
#include <QPixmap>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QCryptographicHash>
//y%dcunyd6x202lmfm9=-y7#bd-w(ro4*(9u$0i-3#$txwbkzg$
const char* MD5_KEY = "*&^%$#@b.v+h-b*g/h@n!h#n$d^ssx,.kl<kl";
//const char* HOST = "http://192.168.0.152:8000";
//const char* HOST = "http://code.edoyun.com:9530";
const char* HOST = "http://127.0.0.1:19527";
bool LOGIN_STATUS = false;

LoginForm::LoginForm(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::LoginForm),
    auto_login_id(-1)//定时器id
{
    record = new RecordFile("edoyun.dat");
    ui->setupUi(this);
    this->setWindowFlag(Qt::FramelessWindowHint);
    //头像进行缩放
    ui->nameEdit->setPlaceholderText(u8"用户名/手机号/邮箱");
    ui->nameEdit->setFrame(false);
    ui->nameEdit->installEventFilter(this);
    ui->pwdEdit->setPlaceholderText(u8"填写密码");
    ui->pwdEdit->setFrame(false);
    ui->pwdEdit->setEchoMode(QLineEdit::Password);
    ui->pwdEdit->installEventFilter(this);
    ui->forget->installEventFilter(this);
    net = new QNetworkAccessManager(this);
    connect(net, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(slots_login_request_finshed(QNetworkReply*)));
    info.setWindowFlag(Qt::FramelessWindowHint);
    info.setWindowModality(Qt::ApplicationModal);
    QSize sz = size();
    info.move((sz.width() - info.width()) / 2, (sz.height() - info.height()) / 2);
    load_config();
}

LoginForm::~LoginForm()
{
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    delete record;
    delete ui;
    delete net;
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
}

bool LoginForm::eventFilter(QObject* watched, QEvent* event)
{
    if(ui->pwdEdit == watched)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->pwdEdit->setStyleSheet("color: rgb(251, 251, 251);background-color: transparent;");
        }
        else if(event->type() == QEvent::FocusOut)
        {
            if(ui->pwdEdit->text().size() == 0)
            {
                ui->pwdEdit->setStyleSheet("color: rgb(71, 75, 94);background-color: transparent;");
            }
        }
    }
    else if(ui->nameEdit == watched)
    {
        if(event->type() == QEvent::FocusIn)
        {
            ui->nameEdit->setStyleSheet("color: rgb(251, 251, 251);background-color: transparent;");
        }
        else if(event->type() == QEvent::FocusOut)
        {
            if(ui->nameEdit->text().size() == 0)
            {
                ui->nameEdit->setStyleSheet("color: rgb(71, 75, 94);background-color: transparent;");
            }
        }
    }
    if((ui->forget == watched) && (event->type() == QEvent::MouseButtonPress))
    {
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
        QDesktopServices::openUrl(QUrl(QString(HOST) + "/forget"));
    }
    return QWidget::eventFilter(watched, event);
}

void LoginForm::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == auto_login_id)
    {
        killTimer(auto_login_id);
        QJsonObject& root = record->config();
        QString user = root["user"].toString();
        QString pwd = root["password"].toString();
        check_login(user, pwd);
    }
}

void LoginForm::on_logoButton_released()
{
    if(ui->logoButton->text() == u8"取消自动登录")
    {
        killTimer(auto_login_id);
        auto_login_id = -1;
        ui->logoButton->setText(u8"登录");
    }
    else
    {
        QString user = ui->nameEdit->text();
        //检查用户名的有效性
        if(user.size() == 0 || user == u8"用户名/手机号/邮箱")
        {
            info.set_text(u8"用户不能为空\r\n请输入用户名", u8"确认").show();
            ui->nameEdit->setFocus();
            return;
        }
        //检查密码的有效性
        QString pwd = ui->pwdEdit->text();
        if(pwd.size() == 0 || pwd == u8"填写密码")
        {
            info.set_text(u8"密码不能为空\r\n请输入密码", u8"确认").show();
            ui->pwdEdit->setFocus();
            return;
        }
        check_login(user, pwd);
    }
}

void LoginForm::on_remberPwd_stateChanged(int state)
{
    //记住密码状态改变
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    record->config()["remember"] = state == Qt::Checked;
    if(state == Qt::Checked)
    {
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    }
    else
    {
        //ui->autoLoginCheck->setChecked(false);//关闭记住密码，则取消自动登录
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    }
}

void LoginForm::slots_autoLoginCheck_stateChange(int state)
{
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    record->config()["auto"] = state == Qt::Checked;
    if(state == Qt::Checked)
    {
        record->config()["remember"] = true;
        ui->remberPwd->setChecked(true);//自动登录会开启记住密码
        //ui->remberPwd->setCheckable(false);//禁止修改状态
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    }
    else
    {
        ui->remberPwd->setCheckable(true);//启动修改状态
        //qDebug() << __FILE__ << "(" << __LINE__ << "):";
    }
    //qDebug() << __FILE__ << "(" << __LINE__ << "):";
}

void LoginForm::slots_login_request_finshed(QNetworkReply* reply)
{
    this->setEnabled(true);
    bool login_success = false;
    if(reply->error() != QNetworkReply::NoError)
    {
        info.set_text(u8"登录发生错误\r\n" + reply->errorString(), u8"确认").show();
        return;
    }
    QByteArray data = reply->readAll();
    qDebug() << data;
    QJsonParseError json_error;
    QJsonDocument doucment = QJsonDocument::fromJson(data, &json_error);
    qDebug() << "json error = "<<json_error.error;
    if (json_error.error == QJsonParseError::NoError)
    {
        if (doucment.isObject())
        {
            const QJsonObject obj = doucment.object();
            if (obj.contains("status") && obj.contains("message"))
            {
                QJsonValue status = obj.value("status");
                QJsonValue message = obj.value("message");
                if(status.toInt(-1) == 0) //登录成功
                {
                    //TODO:token 要保存并传递widget 用于保持在线状态
                    LOGIN_STATUS = status.toInt(-1) == 0;
                    emit login(record->config()["user"].toString(), QByteArray());
                    hide();
                    login_success = true;
                    char tm[64] = "";
                    time_t t;
                    time(&t);
                    strftime(tm, sizeof(tm), "%Y-%m-%d %H:%M:%S", localtime(&t));
                    record->config()["date"] = QString(tm);//更新登录时间
                    record->save();
                }
            }
        }
    }
    else
    {
        //qDebug() << "json error:" << json_error.errorString();
        info.set_text(u8"登录失败\r\n无法解析服务器应答！", u8"确认").show();
    }
    if(!login_success)
    {
        info.set_text(u8"登录失败\r\n用户名或者密码错误！", u8"确认").show();
    }
    reply->deleteLater();
}

QString getTime()
{
    time_t t = 0;
    time (&t);
    return QString::number(t);
}

bool LoginForm::check_login(const QString& user, const QString& pwd)
{
    QCryptographicHash md5(QCryptographicHash::Md5);
    QNetworkRequest request;
    QString url = QString(HOST) + "/login?";
    QString salt = QString::number(QRandomGenerator::global()->bounded(10000, 99999));
    QString time = getTime();
    qDebug().nospace()<< __FILE__ << "(" << __LINE__ << "):" <<time + MD5_KEY + pwd + salt;
    md5.addData((time + MD5_KEY + pwd + salt).toUtf8());
    QString sign = md5.result().toHex();
    url += "time=" + time + "&";
    url += "salt=" + salt + "&";
    url += "user=" + user + "&";
    url += "sign=" + sign;
    //qDebug() << url;
    request.setUrl(QUrl(url));
    record->config()["password"] = ui->pwdEdit->text();
    record->config()["user"] = ui->nameEdit->text();
    this->setEnabled(false);
    net->get(request);
    return true;
    /*
    LOGIN_STATUS = true;
    emit login(record->config()["user"].toString(), QByteArray());
    hide();
    char tm[64] = "";
    time_t t;
    ::time(&t);
    strftime(tm, sizeof(tm), "%Y-%m-%d %H:%M:%S", localtime(&t));
    record->config()["date"] = QString(tm);//更新登录时间
    record->save();
    return false;*/
}

void LoginForm::load_config()
{
    connect(ui->autoLoginCheck, SIGNAL(stateChanged(int)),
            this, SLOT(slots_autoLoginCheck_stateChange(int)));
    QJsonObject& root = record->config();
    ui->remberPwd->setChecked(root["remember"].toBool());
    ui->autoLoginCheck->setChecked(root["auto"].toBool());
    QString user = root["user"].toString();
    QString pwd = root["password"].toString();
    ui->nameEdit->setText(user);
    ui->pwdEdit->setText(pwd);
    qDebug() << "auto:" << root["auto"].toBool();
    qDebug() << "remember:" << root["remember"].toBool();
    if(root["auto"].toBool()) //如果开启了自动登录，则检查用户名和密码是否ok
    {
        qDebug() << __FILE__ << "(" << __LINE__ << "):user=" << user;
        qDebug() << __FILE__ << "(" << __LINE__ << "):pwd=" << pwd;
        if(user.size() > 0 && pwd.size() > 0)
        {
            ui->logoButton->setText(u8"取消自动登录");
            auto_login_id = startTimer(3000);//给3秒的时间，方便用户终止登录过程
        }
    }
}

void LoginForm::mouseMoveEvent(QMouseEvent* event)
{
    move(event->globalPos() - position);
}

void LoginForm::mousePressEvent(QMouseEvent* event)
{
    position = event->globalPos() - this->pos();
}

void LoginForm::mouseReleaseEvent(QMouseEvent* /*event*/)
{
}
