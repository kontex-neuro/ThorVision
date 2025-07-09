#include <QtGui>
#include <QtQml>

#include "CameraModel.h"
#include "GstVideoSink.h"
#include "ImageProvider.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    GstVideoSink video_sink;
    auto provider = new ImageProvider();

    engine.addImageProvider("video", provider);
    video_sink.setImageProvider(provider);

    // engine.rootContext()->setContextProperty("video", &video_sink);

    CameraModel camera_model;

    camera_model.add_camera("Camera 1");
    camera_model.add_camera("Camera 2");
    camera_model.add_camera("Camera 3");
    camera_model.add_camera("Camera 4");

    engine.rootContext()->setContextProperty("CameraModel", &camera_model);


    const QUrl url(QStringLiteral("thorvision/src/main.qml"));
    engine.load(url);

    if (engine.rootObjects().isEmpty()) return -1;

    return app.exec();
}