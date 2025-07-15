#include "ImageProvider.h"

ImageProvider::ImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

void ImageProvider::setImage(const QImage &img)
{
    QMutexLocker locker(&mutex);
    currentImage = img;
}

QImage ImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    QMutexLocker locker(&mutex);
    if (size) *size = currentImage.size();

    // fallback
    if (currentImage.isNull()) return QImage(320, 240, QImage::Format_RGB888);

    return currentImage;
}
