#include "widget.h"
#include "loginform.h"
#include <QApplication>
#include "vlchelper.h"
#include <QDebug>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QApplication::addLibraryPath("./plugins");
    Widget w;
    LoginForm login;
    w.hide();
    login.show();
    login.connect(&login, SIGNAL(login(const QString&, const QByteArray&)),
                  &w, SLOT(on_show(const QString&, const QByteArray&)));
    qDebug() << __FILE__ << "(" << __LINE__ << "):";
    int ret = a.exec();
    qDebug() << __FILE__ << "(" << __LINE__ << "):";
    return ret;
}
