#include "CameraModel.h"

CameraModel::CameraModel(QObject *parent) : QAbstractListModel(parent) {}

CameraModel::~CameraModel() {}

int CameraModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return _cameras.count();
}

QVariant CameraModel::data(const QModelIndex &index, int role) const
{
    auto row = index.row();

    if (!index.isValid() || row < 0 || row >= _cameras.count()) {
        return QVariant();
    }

    // A model can return data for different roles.
    // The default role is the display role.
    // it can be accesses in QML with "model.display"
    const CameraItem &item = _cameras[index.row()];

    switch (role) {
    case NameRole: return item.name();
    // case CapRole: return item.caps();
    case CapRole: return item.caps().isEmpty() ? QVariant() : item.caps().first();
    // case CodecRole: return item.codecs();
    case CodecRole: return item.codecs().isEmpty() ? QVariant() : item.codecs().first();
    default: return QVariant();
    }
}

QHash<int, QByteArray> CameraModel::roleNames() const
{
    // QHash<int, QByteArray> roles;
    // roles[NameRole] = "name";
    // return roles;
    return {
        {NameRole, "name"},
        {CapRole, "caps"},
        {CodecRole, "codecs"},
    };
}

// void CameraModel::add_camera(const QString &name)
void CameraModel::add_camera(
    const QString &name, const QVector<QString> &caps, const QVector<QString> &codecs
)
{
    beginInsertRows(QModelIndex(), _cameras.size(), _cameras.size());

    // auto camera_item = CameraItem(name, caps, codecs);
    _cameras.append(CameraItem(name, caps, codecs));

    endInsertRows();
}


void CameraModel::set_name(const int index, const QString &name) { _cameras[index].set_name(name); }

void CameraModel::set_cap(const int index, const QString &cap) { _cameras[index].set_cap(cap); }

void CameraModel::set_codec(const int index, const QString &codec)
{
    _cameras[index].set_codec(codec);
}

QString CameraModel::name(int index) const
{
    return index >= 0 && index < _cameras.size() ? _cameras[index].name() : QString();
}

QStringList CameraModel::caps(int index) const
{
    return index >= 0 && index < _cameras.size() ? _cameras[index].caps().toList() : QStringList();
}

QStringList CameraModel::codecs(int index) const
{
    return index >= 0 && index < _cameras.size() ? _cameras[index].codecs().toList()
                                                 : QStringList();
}