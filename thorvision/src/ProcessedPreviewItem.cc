#include "ProcessedPreviewItem.h"

#include <QPainter>

ProcessedPreviewItem::ProcessedPreviewItem(QQuickItem *parent) : QQuickPaintedItem(parent) {}

ProcessedPreviewItem::~ProcessedPreviewItem() { watch(false); }

void ProcessedPreviewItem::watch(bool watching)
{
    if (_client && !_camera.isEmpty()) _client->watch_preview(_camera, watching);
}

void ProcessedPreviewItem::set_client(ExtensionClient *client)
{
    if (client == _client) return;
    watch(false);
    if (_client) disconnect(_client, nullptr, this, nullptr);
    _client = client;
    if (_client) {
        connect(_client, &ExtensionClient::preview_updated, this, [this](const QString &camera) {
            if (camera == _camera) update();
        });
        connect(_client, &ExtensionClient::preview_freshness_changed, this, [this](const QString &camera) {
            if (camera == _camera) emit fresh_changed();
        });
    }
    watch(true);
    emit client_changed();
    emit fresh_changed();
    update();
}

void ProcessedPreviewItem::set_camera(const QString &camera)
{
    if (camera == _camera) return;
    watch(false);
    _camera = camera;
    watch(true);
    emit camera_changed();
    emit fresh_changed();
    update();
}

void ProcessedPreviewItem::paint(QPainter *painter)
{
    if (!_client) return;
    auto image = _client->preview(_camera);
    if (image.isNull()) return;
    auto target = QRectF(QPointF(0, 0), image.size().scaled(size().toSize(), Qt::KeepAspectRatio));
    target.moveCenter(boundingRect().center());
    painter->fillRect(boundingRect(), Qt::black);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(target, image);
}
