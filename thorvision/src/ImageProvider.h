#pragma once

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <unordered_map>

class ImageProvider : public QQuickImageProvider
{
public:
    ImageProvider();
    ~ImageProvider() = default;

    virtual QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize)
        override;

    void setImage(const QString &id, const QImage &img);

private:
    // QMap<QString, QImage> _images;
    std::unordered_map<QString, QImage> _images;
    QMutex mutex;
};
