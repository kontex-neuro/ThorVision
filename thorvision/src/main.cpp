#include <QIcon>
#include <QMediaPlayer>
#include <QtGui>
#include <QtQml>

#include "GstVideoSink.h"
#include "ImageProvider.h"


int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon("resources/xdaq-icon.ico"));
    QQmlApplicationEngine engine;

    GstVideoSink video_sink;
    auto provider = new ImageProvider();

    engine.addImageProvider("video", provider);
    video_sink.setImageProvider(provider);

    // engine.rootContext()->setContextProperty("video", &video_sink);
    // engine.rootContext()->setContextProperty("backend", &videoSink);

    // QObject::connect(
    //     &video_sink,
    //     &GstVideoSink::newImageReady,
    //     provider,
    //     [provider](const QImage &img) { provider->setImage(img); }
    // );

    // auto player = new QMediaPlayer;
    // player->setSource(QUrl("gst-pipeline: videotestsrc ! autovideosink"));
    // player->play();

    const QUrl url(QStringLiteral("thorvision/src/main.qml"));
    engine.load(url);
    if (engine.rootObjects().isEmpty()) return -1;

    return app.exec();
}