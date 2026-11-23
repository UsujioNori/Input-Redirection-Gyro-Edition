#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif

#include "mainwidget.h"
#include "gpmanager.h"
#include <QTimer>
#include <QThread>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationDisplayName(QStringLiteral("Input Redirection Gyro Edition"));
    Widget w;
    GamepadMonitor m(&w);

    w.show();
#ifdef ENABLE_SDL_PRO3
    // SDL polling and Qt gamepad callbacks share the UI thread.
    QTimer timer;
    timer.setTimerType(Qt::PreciseTimer);
    QObject::connect(&timer, &QTimer::timeout, &worker, &Worker::sendFrame);
    timer.start(10);
    return a.exec();
#else
    QThread* thread = new QThread();
    QTimer timer;
    timer.setInterval(20);

    timer.moveToThread(thread);
    worker.moveToThread(thread);

    QObject::connect(thread, SIGNAL (started()), &timer, SLOT (start()));
    QObject::connect(&worker, SIGNAL (finished()), thread, SLOT (quit()));
    QObject::connect(thread, SIGNAL (finished()), thread, SLOT (deleteLater()));
    QObject::connect(&timer, SIGNAL(timeout()), &worker, SLOT(sendFrame()));
    thread->start();
    a.exec();
    worker.closeThread();
    return 0;
#endif
}
