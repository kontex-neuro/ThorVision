#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

class ImageProvider : public QQuickImageProvider
{
public:
    ImageProvider();
    ~ImageProvider() = default;

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

    void setImage(const QImage &img);

private:
    QImage currentImage;
    QMutex mutex;
};
