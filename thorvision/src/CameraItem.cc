#include "CameraItem.h"

CameraItem::CameraItem() {}

CameraItem::CameraItem(
    const QString &name, const QVector<QString> &caps, const QVector<QString> &codecs
)
{
    _name = name;
    _caps = caps;
    _codecs = codecs;
}

CameraItem::~CameraItem() {}

QString CameraItem::name() const { return _name; }

QVector<QString> CameraItem::caps() const { return _caps; }

QVector<QString> CameraItem::codecs() const { return _codecs; }

void CameraItem::set_name(const QString &name) { _name = name; }

void CameraItem::set_cap(const QString &cap) { _cap = cap; }

void CameraItem::set_codec(const QString &codec) { _codec = codec; }