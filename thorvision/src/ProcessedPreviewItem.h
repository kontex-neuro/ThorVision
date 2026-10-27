#pragma once

#include <QPointer>
#include <QQuickPaintedItem>

#include "ExtensionClient.h"

// Device-rendered analytics preview for one camera. It shows the JPEG the device drew on the
// exact frame the callback processed, never QML shapes over the raw stream, and only while
// that image is fresh; otherwise it is empty and the raw video behind it shows through.
class ProcessedPreviewItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(ExtensionClient *client READ client WRITE set_client NOTIFY client_changed)
    Q_PROPERTY(QString camera READ camera WRITE set_camera NOTIFY camera_changed)
    Q_PROPERTY(bool fresh READ fresh NOTIFY fresh_changed)
    Q_PROPERTY(QVariantMap header READ header NOTIFY fresh_changed)

public:
    explicit ProcessedPreviewItem(QQuickItem *parent = nullptr);
    ~ProcessedPreviewItem() override;

    ExtensionClient *client() const { return _client; }
    void set_client(ExtensionClient *client);
    QString camera() const { return _camera; }
    void set_camera(const QString &camera);
    bool fresh() const { return _client && _client->preview_fresh(_camera); }
    QVariantMap header() const { return _client ? _client->preview_header(_camera) : QVariantMap{}; }

    void paint(QPainter *painter) override;

signals:
    void client_changed();
    void camera_changed();
    void fresh_changed();

private:
    void watch(bool watching);

    QPointer<ExtensionClient> _client;
    QString _camera;
};
