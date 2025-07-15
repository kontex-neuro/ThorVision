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

    // CameraModel camera_model;
    auto camera_model = new CameraModel(&app);

    QString name = "Camera 1";
    QVector<QString> caps = {"Full HD @ 120FPS", "Full HD @ 30FPS"};
    QVector<QString> codecs = {"H.265", "M-JPEG"};

    QString name_2 = "Camera 2";
    QVector<QString> caps_2 = {"HD @ 60FPS", "HD @ 30FPS"};
    QVector<QString> codecs_2 = {"M-JPEG", "H.265"};

    camera_model->add_camera(name, caps, codecs);
    camera_model->add_camera(name_2, caps_2, codecs_2);

    // qmlRegisterUncreatableType<CameraModel, 1>(
    //     "com.test.CameraModel", 1, 0, "CameraModel", "Cannot create CameraModel"
    // );
    // qmlRegisterType<CameraModel>("Custom", 1, 0, "CameraModel");

    // camera_model.add_camera(name, caps, codecs);
    // camera_model.add_camera(name_2, caps_2, codecs_2);

    // engine.rootContext()->setContextProperty("CameraModel", &camera_model);
    engine.rootContext()->setContextProperty("CameraModel", camera_model);

    const QUrl url(QStringLiteral("thorvision/src/main.qml"));
    engine.load(url);

    if (engine.rootObjects().isEmpty()) return -1;

    return app.exec();
}