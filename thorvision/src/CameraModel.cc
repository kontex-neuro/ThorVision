#include "CameraModel.h"

CameraModel::CameraModel(QObject *parent) : QAbstractListModel(parent) {}

CameraModel::~CameraModel() {}

int CameraModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_data.count();
}

QVariant CameraModel::data(const QModelIndex &index, int role) const
{
    auto row = index.row();

    if (row < 0 || row >= m_data.count()) {
        return QVariant();
    }

    // A model can return data for different roles.
    // The default role is the display role.
    // it can be accesses in QML with "model.display"

    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return m_data.value(row);
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
        {CapRole, "cap"},
        {CodecRole, "codec"},
    };
}

void CameraModel::add_camera(const QString &name)
{
    beginInsertRows(QModelIndex(), m_data.size(), m_data.size());
    m_data.append(name);
    endInsertRows();
}
