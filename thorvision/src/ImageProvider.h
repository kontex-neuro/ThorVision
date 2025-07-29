#pragma once

#include <QImage>
#include <QMap>
#include <QMutex>
#include <QQuickImageProvider>
#include <QString>

#include "xdaqmetadata/xdaqmetadata.h"

class ImageProvider : public QQuickImageProvider
{
public:
    ImageProvider();
    ~ImageProvider() = default;

    virtual QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize)
        override;

    void setImage(const QString &id, const QImage &img, const XDAQFrameData &metadata);

    XDAQFrameData metadata(const QString &id);

private:
    QMutex mutex;
    QMap<QString, QImage> _images;
    QMap<QString, XDAQFrameData> _metadata;
};
