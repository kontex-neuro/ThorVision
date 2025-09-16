#include "ImageProvider.h"

#include <spdlog/spdlog.h>

ImageProvider::ImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

void ImageProvider::setImage(const QString &id, const QImage &img, const XDAQFrameData &metadata)
{
    QMutexLocker locker(&mutex);

    _images[id] = img;
    _metadata[id] = metadata;
}

XDAQFrameData ImageProvider::metadata(const QString &id)
{
    QMutexLocker locker(&mutex);

    auto key = id;
    auto queryIndex = key.indexOf('?');
    if (queryIndex != -1) {
        key = key.left(queryIndex);
    }
    return _metadata.value(key, XDAQFrameData{0, 0, 0, 0, 0, 0});
}

QImage ImageProvider::requestImage(const QString &id, QSize *size, [[maybe_unused]] const QSize &requestedSize)
{
    QMutexLocker locker(&mutex);

    auto key = id;
    auto queryIndex = key.indexOf('?');
    if (queryIndex != -1) {
        key = key.left(queryIndex);
    }

    if (_images.contains(key)) {
        auto img = _images[key];
        if (size) *size = img.size();
        return img;
    }

    return QImage();
}
