#ifndef WIDGET_H
#define WIDGET_H
#include <windows.h>
#include <QWidget>
#include <QSlider>
#include <QDropEvent>
#include <QList>
#include <QListWidgetItem>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonDocument>
#include "vlchelper.h"
#include "ssltool.h"
#include "qmediaplayer.h"
#include "infoform.h"
#include "messageform.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget* parent = nullptr);
    ~Widget();
    virtual void timerEvent(QTimerEvent* event);
protected:
    void save_default_rect_info();
    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);
    void handleTimeout(int nTimerID);
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mousePressEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual bool eventFilter(QObject* watched, QEvent* event);
    virtual void keyPressEvent(QKeyEvent* event);
private:
    void setSlider(QSlider* slider, int nMin, int nMax, int nStep);
    //设置当前播放的时间
    void setTime(int hour, int minute, int seconds);
    void setTime(int64_t tm);
    //设置媒体总的时长
    void setTime2(int hour, int minute, int seconds);
    void setTime2(int64_t tm);
    void paintLine();
    void init_media();
signals:
    void update_pos(double pos);
    void open(const QUrl& url);
    void play();
    void pause();//暂停
    void stop();//停止播放
    void close_media();//关闭媒体
    void seek(double pos);
    void set_position(int64_t pos);//设置播放位置 单位毫秒
    void set_position(//设置播放位置，时分秒毫秒
        int hour, int minute, int second, int millisecond
    );
    void set_scale(float scale);//设置播放倍率
    //取指定位置(单位毫秒)的一帧图片（用于预览）
    void pick_frame(QImage& frame, int64_t pos);
    //改变播放窗口大小
    void set_size(const QSize& sz);
    void volume(int vol);
    void silence();
public slots:
    void on_show(const QString& nick, const QByteArray& head);
    void slot_connect_clicked();//联系售后
protected slots:
    void on_preButton_clicked();
    void on_playButton_clicked();
    void on_nextButton_clicked();
    void on_volumeButton_clicked();
    void on_scaleButton_clicked();
    void on_horizontalSlider_sliderPressed();
    void on_horizontalSlider_sliderReleased();
    void on_horizontalSlider_rangeChanged(int min, int max);
    void on_horizontalSlider_valueChanged(int value);
    void on_media_position(double pos);
    void on_media_media_status(QMediaPlayer::PlayerStatus s);
    void slots_login_request_finshed(QNetworkReply* reply);
private slots:
    void on_listWidget_itemDoubleClicked(QListWidgetItem* item);
    void on_volumeSlider_sliderReleased();
    void on_volumeSlider_valueChanged(int value);
    void on_showhideList_pressed();
    void on_closeButton_released();
    void on_fangdaButton_clicked();
    void on_fullscreenBtn_clicked();
    void on_stopButton_clicked();
    void on_time2_clicked();
    void on_time1_5_clicked();
    void on_time1_clicked();
    void on_time0_5_clicked();

    void on_miniButton_clicked();

private:
    void on_fastButton_clicked();
    void on_slowButton_clicked();
    bool keep_activity();
private:
    //尺寸助手
    class SizeHelper
    {
    public:
        SizeHelper(Widget* ui);
        ~SizeHelper();
        //当调整完大小的时候更新
        void update(int nWidth, int nHeight);
        //获取当前的鼠标光标类型 0 正常 1 左上 2 顶 3 右上 4 右 5 右下 6 底 7 左下 8 左
        void modify_mouse_cousor(const QPoint& point);
        //设置false禁用本功能（全屏时禁用）true开启功能，默认为true
        void set_enable(bool isable = true);
        int type()
        {
            return index;
        }
        void press(const QPoint& point);
        void move(const QPoint& point);
        void release();
        void init_size_info(QWidget* widget);
        void set_full_rect(const QString& name, const QRect& rect);
        void set_max_rect(const QString& name, const QRect& rect);
        void set_org_rect(const QString& name, const QRect& rect);
        void set_last_rect(const QString& name, const QRect& rect);
        void set_full_hide(const QString& name, bool is_hide = true);
        void set_max_hide(const QString& name, bool is_hide = true);
        void set_auto_hide(const QString& name, int hide_status = 1);
        void full_size();
        void org_size();
        void max_size();
        int  cur_status() const;
        void auto_hide(bool hidden = true);
    protected:
        struct SizeInfo
        {
            bool is_full_hide;//全屏隐藏
            bool is_max_hide;//最大化隐藏
            int auto_hide_status;//全屏时自动隐藏的控件 0 不隐藏 1 隐藏 2 不参与（该控件在全屏模式下不存在)
            QRect org_rect;//原始尺寸
            QRect max_rect;//最大化尺寸
            QRect full_rect;//全屏尺寸
            QRect last_rect;//上一个尺寸
            QWidget* widget;//控件
            SizeInfo()
            {
                widget = NULL;
                is_full_hide = false;
                is_max_hide = false;
            }
        };
    protected:
        //窗口调整属性
        QRect size_rect[8];//8个区域，会触发鼠标变化，用于改变ui整体窗口大小
        bool isabled;//默认为true
        int curent_coursor;//当前光标类型，默认为0，表示普通光标
        QCursor* cursors[9];//光标
        int index;//当前起效的光标
        QPoint point;//按下的起点
        bool pressed;//按下状态
        Widget* ui;
        QMap<QString, SizeInfo> sub_item_size;
        int status;//当前屏幕状态0 普通 1 最大化 2 全屏
    };
private:
    Ui::Widget* ui;
    QList<QUrl> mediaList;
    //vlchelper* vlc;
    SslTool ssl_tool;
    int current_play;
    int timerID;
    int timesCount;
    int timesID;//倍速按钮定时器
    int volumeSliderTimerID;
    int fullScreenTimerID;
    int keep_activity_timerID;
    int volumeCount;
    QPoint position;
    float rate;
    bool slider_pressed;
    qint16 tick;
    SizeHelper helper;
    InfoForm info;
    MessageForm* full_head;//全屏时的头部半透明框
    int screentopTimerID;
    QNetworkAccessManager* net;
    QString nick;//昵称
};
#endif // WIDGET_H
