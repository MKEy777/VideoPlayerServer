#include "vlchelper.h"
#include <functional>
#include <QDebug>
#include <QTime>
#include <QRandomGenerator>
#include <QImage>
//vlc 播放进度回调
//拖拽进度、播放速度、停止播放
using namespace std;
using namespace std::placeholders;
void libvlc_log_callback(void* data, int level, const libvlc_log_t* ctx,
                         const char* fmt, va_list args)
{
    //qDebug() << "log level:" << level;
}
vlchelper::vlchelper(QWidget* widget)
    : m_logo(":/ico/UI/ico/128-128.png")
{
    const char* const args[] =
    {
        "--sub-filter=logo",
        "--sub-filter=marq"
    };
    qDebug() << __FUNCTION__ << ":" << __LINE__;
    m_instance = libvlc_new(2, args);
    if(m_instance != NULL)
    {
        qDebug() << __FUNCTION__ << ":" << __LINE__;
        m_media = new vlcmedia(m_instance);
    }
    else
    {
        qDebug() << __FUNCTION__ << ":" << __LINE__;
        m_media = NULL;
        throw QString("是否没有安装插件？！！");
    }
    libvlc_log_set(m_instance, libvlc_log_callback, this);
    m_player = NULL;
    m_hWnd = (HWND)widget->winId();
    winHeight = widget->frameGeometry().height();
    winWidth = widget->frameGeometry().width();
    m_widget = widget;
    qDebug() << "*winWidth:" << winWidth;
    qDebug() << "*winHeight:" << winHeight;
    m_isplaying = false;
    m_ispause = false;
    m_volume = 100;
}

vlchelper::~vlchelper()
{
    if(m_player != NULL)
    {
        stop();
        libvlc_media_player_set_hwnd(m_player, NULL);
        libvlc_media_player_release(m_player);
        m_player = NULL;
    }
    if(m_media != NULL)
    {
        m_media->free();
        m_media = NULL;
    }
    if(m_instance != NULL)
    {
        libvlc_release(m_instance);
        m_instance = NULL;
    }
}

int vlchelper::prepare(const QString& strPath)
{
    qDebug() << strPath;
    //m_media = libvlc_media_new_location(
    //              m_instance, strPath.toStdString().c_str());
    *m_media = strPath;
    if(m_media == NULL)
    {
        return -1;
    }
    if(m_player != NULL)
    {
        libvlc_media_player_release(m_player);
    }
    m_player = libvlc_media_player_new_from_media(*m_media);
    if(m_player == NULL)
    {
        return -2;
    }
    m_duration = libvlc_media_get_duration(*m_media);
    libvlc_media_player_set_hwnd(m_player, m_hWnd);
    libvlc_audio_set_volume(m_player, m_volume);
    libvlc_video_set_aspect_ratio(m_player, "16:9");
    if(text.size() > 0)
    {
        set_float_text();
    }
    m_ispause = false;//初始化暂停状态
    m_isplaying = false;//初始化播放状态
    m_issilence = false;//初始是无静音状态
    if(m_widget->frameGeometry().height() != winHeight)
    {
        winHeight = m_widget->frameGeometry().height();
    }
    if(m_widget->frameGeometry().width() != winWidth)
    {
        winWidth = m_widget->frameGeometry().width();
    }
    qDebug() << "*winWidth:" << winWidth;
    qDebug() << "*winHeight:" << winHeight;
    return 0;
}

int vlchelper::play()
{
    if(m_player == NULL)
    {
        return -1;
    }
    if(m_ispause)//如果是暂停，则直接使用play来恢复
    {
        int ret = libvlc_media_player_play(m_player);
        if(ret == 0)//如果执行成功，则改变暂停状态
        {
            m_ispause = false;
            m_isplaying = true;
        }
        return ret;
    }
    if((m_player == NULL) ||//没有设置媒体
            (m_media->path().size() <= 0))
    {
        m_ispause = false;
        m_isplaying = false;
        return -2;
    }
    m_isplaying = true;
    libvlc_video_set_mouse_input(m_player, 0); //使得vlc不处理鼠标交互，方便qt处理
    libvlc_video_set_key_input(m_player, 0); //使得vlc不处理键盘交互，方便qt处理
    libvlc_set_fullscreen(m_player, 1);
    return libvlc_media_player_play(m_player);
}
int vlchelper::pause()
{
    if(m_player == NULL)
    {
        return -1;
    }
    libvlc_media_player_pause(m_player);
    m_ispause = true;
    m_isplaying = false;
    return 0;
}
int vlchelper::stop()
{
    if(m_player != NULL)
    {
        libvlc_media_player_stop(m_player);
        m_isplaying = false;
        return 0;
    }
    return -1;
}
int vlchelper::volume(int vol)
{
    if(m_player == NULL)
    {
        return -1;
    }
    if(vol == -1)
    {
        return m_volume;
    }
    int ret = libvlc_audio_set_volume(m_player, vol);
    if(ret == 0)
    {
        m_volume = vol;
        return m_volume;
    }
    return ret;
}

int vlchelper::silence()
{
    if(m_player == NULL)
    {
        return -1;
    }
    if(m_issilence)
    {
        //恢复
        libvlc_audio_set_mute(m_player, 0);
        m_issilence = false;
    }
    else
    {
        //静音
        m_issilence = true;
        libvlc_audio_set_mute(m_player, 1);
    }
    return m_issilence;
}

bool vlchelper::isplaying()
{
    return m_isplaying;
}

bool vlchelper::ismute()
{
    if(m_player && m_isplaying)
    {
        return libvlc_audio_get_mute(m_player) == 1;
    }
    return false;
}

libvlc_time_t vlchelper::gettime()
{
    if(m_player == NULL)
    {
        return -1;
    }
    return libvlc_media_player_get_time(m_player);
}

libvlc_time_t vlchelper::getduration()
{
    if(m_media == NULL)
    {
        return -1;
    }
    if(m_duration == -1)
    {
        m_duration = libvlc_media_get_duration(*m_media);
    }
    return m_duration;
}

int vlchelper::settime(libvlc_time_t time)
{
    if(m_player == NULL)
    {
        return -1;
    }
    libvlc_media_player_set_time(m_player, time);
    return 0;
}

int vlchelper::set_play_rate(float rate)
{
    if(m_player == NULL)
    {
        return -1;
    }
    return libvlc_media_player_set_rate(m_player, rate);
}

float vlchelper::get_play_rate()
{
    if(m_player == NULL)
    {
        return -1.0;
    }
    return libvlc_media_player_get_rate(m_player);
}

void vlchelper::init_logo()
{
    //libvlc_video_set_logo_int(m_player, libvlc_logo_file, m_logo.handle());
    libvlc_video_set_logo_string(m_player, libvlc_logo_file, "128-128.png"); //Logo 文件名
    libvlc_video_set_logo_int(m_player, libvlc_logo_x, 0); //logo的 X 坐标。
    //libvlc_video_set_logo_int(m_player, libvlc_logo_y, 0); // logo的 Y 坐标。
    libvlc_video_set_logo_int(m_player, libvlc_logo_delay, 100);//标志的间隔图像时间为毫秒,图像显示间隔时间 0 - 60000 毫秒。
    libvlc_video_set_logo_int(m_player, libvlc_logo_repeat, -1); // 标志logo的循环,  标志动画的循环数量。-1 = 继续, 0 = 关闭
    libvlc_video_set_logo_int(m_player, libvlc_logo_opacity, 122);
    // logo 透明度 (数值介于 0(完全透明) 与 255(完全不透明)
    libvlc_video_set_logo_int(m_player, libvlc_logo_position, 5);
    //1 (左), 2 (右), 4 (顶部), 8 (底部), 5 (左上), 6 (右上), 9 (左下), 10 (右下),您也可以混合使用这些值，例如 6=4+2    表示右上)。
    libvlc_video_set_logo_int(m_player, libvlc_logo_enable, 1); //设置允许添加logo
}

void vlchelper::init_text(const QString& text)
{
    this->text = text;
}

void vlchelper::update_logo()
{
    static int alpha = 0;
    //static int pos[] = {1, 5, 4, 6, 2, 10, 8, 9};
    int height = QRandomGenerator::global()->bounded(20, winHeight - 20);
    libvlc_video_set_logo_int(m_player, libvlc_logo_y, height); // logo的 Y 坐标。
    int width = QRandomGenerator::global()->bounded(20, winWidth - 20);
    libvlc_video_set_logo_int(m_player, libvlc_logo_x, width); //logo的 X 坐标。
    libvlc_video_set_logo_int(m_player, libvlc_logo_opacity, (alpha++) % 80 + 20); //透明度
    //libvlc_video_set_logo_int(m_player, libvlc_logo_position, pos[alpha % 8]);
}

void vlchelper::update_text()
{
    static int alpha = 0;
    if(m_player)
    {
        int color = QRandomGenerator::global()->bounded(0x30, 0x60);//R
        color = color << 8 | QRandomGenerator::global()->bounded(0x30, 0x60);//G
        color = color << 8 | QRandomGenerator::global()->bounded(0x30, 0x60);//B
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Color, color);//颜色
        int x = QRandomGenerator::global()->bounded(20, winHeight - 20);//随机位置
        int y = QRandomGenerator::global()->bounded(20, winWidth - 20);
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_X, x);//
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Y, y);
        //随机透明度
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Opacity, (alpha++ % 60) + 10);
    }
}

bool vlchelper::is_text_enable()
{
    if(m_player == NULL)
    {
        return false;
    }
    return libvlc_video_get_marquee_int(m_player, libvlc_marquee_Enable) == 1;
}

bool vlchelper::has_media_player()
{
    return m_player != NULL && (m_media != NULL);
}

void vlchelper::set_float_text()
{
    if(m_player)
    {
        libvlc_video_set_marquee_string(m_player, libvlc_marquee_Text, text.toStdString().c_str());
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Color, 0x404040);
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_X, 0);
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Y, 0);
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Opacity, 100);
        //libvlc_video_set_marquee_int(m_player, libvlc_marquee_Timeout, 100);
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Position, 5);
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Size, 14);
        libvlc_video_set_marquee_int(m_player, libvlc_marquee_Enable, 1);
    }
}

bool vlchelper::is_logo_enable()
{
    if(m_player == NULL)
    {
        return false;
    }
    qDebug() << __FUNCTION__ << " logo enable:" << libvlc_video_get_logo_int(m_player, libvlc_logo_enable);
    return libvlc_video_get_logo_int(m_player, libvlc_logo_enable) == 1;
}

vlcmedia::vlcmedia(libvlc_instance_t* ins)
    : instance(ins)
{
    media = NULL;
    media_instance = new MediaMP4();
}

vlcmedia::~vlcmedia()
{
    if(media)
    {
        free();
    }
    if(media_instance)
    {
        delete media_instance;
        media_instance = NULL;
    }
}

int vlcmedia::open(void* thiz, void** infile, uint64_t* fsize)
{
    vlcmedia* _this = (vlcmedia*)thiz;
    return _this->open(infile, fsize);
}

ssize_t vlcmedia::read(void* thiz, uint8_t* buffer, size_t length)
{
    vlcmedia* _this = (vlcmedia*)thiz;
    return _this->read(buffer, length);
}

int vlcmedia::seek(void* thiz, uint64_t offset)
{
    vlcmedia* _this = (vlcmedia*)thiz;
    return _this->seek(offset);
}

void vlcmedia::close(void* thiz)
{
    vlcmedia* _this = (vlcmedia*)thiz;
    _this->close();
}

vlcmedia& vlcmedia::operator=(const QString& str)
{
    if(media)
    {
        free();
    }
    //libvlc_media_read_cb
    strPath = str;
    media = libvlc_media_new_callbacks(
                instance,
                &vlcmedia::open,
                &vlcmedia::read,
                &vlcmedia::seek,
                &vlcmedia::close,
                this);
    return *this;
}

void vlcmedia::free()
{
    if(media != NULL)
    {
        libvlc_media_release(media);
    }
}

QString vlcmedia::path()
{
    return strPath;
}

int vlcmedia::open(void** infile, uint64_t* fsize)
{
    //"file:///"
    if(media_instance)
    {
        *infile = this;
        int ret = media_instance->open(strPath, fsize);
        media_size = *fsize;
        return ret;
    }
    this->infile.open(strPath.toStdString().c_str() + 8, ios::binary | ios::in);
    this->infile.seekg(0, ios::end);
    *fsize = (uint64_t)this->infile.tellg();
    media_size = *fsize;
    this->infile.seekg(0);
    *infile = this;
    return 0;
}

ssize_t vlcmedia::read(uint8_t* buffer, size_t length)
{
    if(media_instance)
    {
        return media_instance->read(buffer, length);
    }
    //qDebug() << __FUNCTION__ << " length:" << length;
    uint64_t pos = (uint64_t)infile.tellg();
    //qDebug() << __FUNCTION__ << " positon:" << pos;
    if(media_size - pos < length)
    {
        length = media_size - pos;
    }
    infile.read((char*)buffer, length);
    return infile.gcount();
}

int vlcmedia::seek(uint64_t offset)
{
    if(media_instance)
    {
        return media_instance->seek(offset);
    }
    //qDebug() << __FUNCTION__ << ":" << offset;
    infile.clear();
    infile.seekg(offset);
    return 0;
}

void vlcmedia::close()
{
    if(media_instance)
    {
        return media_instance->close();
    }
    //qDebug() << __FUNCTION__;
    infile.close();
}

vlcmedia::operator libvlc_media_t* ()
{
    return media;
}
