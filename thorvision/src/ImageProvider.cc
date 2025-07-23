#include "ImageProvider.h"

#include <spdlog/spdlog.h>

ImageProvider::ImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

void ImageProvider::setImage(const QString &id, const QImage &img)
{
    QMutexLocker locker(&mutex);
    // spdlog::info("setImage() id = {}", id.toStdString());
    _images[id] = img;
}

QImage ImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QMutexLocker locker(&mutex);
    // spdlog::info("requestImage id = {}", id.toStdString());
    // if (size) *size = _images.size();

    // if (_images.contains(id)) {
    //     spdlog::info("requestImage");
    //     auto img = _images[id];
    //     if (size) *size = img.size();
    //     return img;
    // }
    QString key = id;
    int queryIndex = key.indexOf('?');
    if (queryIndex != -1) {
        key = key.left(queryIndex);
    }

    // spdlog::info("requestImage stripped key = {}", key.toStdString());

    if (_images.contains(key)) {
        auto img = _images[key];
        if (size) *size = img.size();
        return img;
    }

    // spdlog::warn("Image not found for key: {}", key.toStdString());

    // fallback
    // if (currentImage.isNull()) return QImage(320, 240, QImage::Format_RGB888);
    return QImage();

    // return currentImage;
}
