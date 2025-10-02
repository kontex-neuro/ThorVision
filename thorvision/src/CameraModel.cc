#include "CameraModel.h"

#include <spdlog/spdlog.h>

CameraModel::CameraModel(QObject *parent) : QAbstractListModel(parent)
{
    _selected_camera_index = -1;
}

CameraModel::~CameraModel() { qDeleteAll(_cameras); }

int CameraModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    spdlog::info("rowCount() = {}", _cameras.count());
    return _cameras.count();
}

QVariant CameraModel::data(const QModelIndex &index, int role) const
{
    auto row = index.row();
    if (!index.isValid() || row < 0 || row >= _cameras.count()) return QVariant();

    const auto &camera = _cameras[row];
    spdlog::info(
        "data() row = {}, role = {}, id = {}, name = {}, cap = {}, codec = {}",
        row,
        role,
        camera->id(),
        camera->name().toStdString(),
        camera->cap().toStdString(),
        camera->codec().toStdString()
    );

    switch (role) {
    case IdRole: return camera->id();
    case CameraItemRole: return QVariant::fromValue(camera);
    case NameRole: return camera->name();
    case CapRole: return camera->cap();
    case CodecRole: return camera->codec();
    case CapsRole: return camera->caps();
    case CodecsRole: return camera->codecs();
    default: return QVariant();
    }
}

QHash<int, QByteArray> CameraModel::roleNames() const
{
    spdlog::info("CameraModel::roleNames()");
    return {
        {IdRole, "id"},
        {CameraItemRole, "camera_item"},
        {NameRole, "name"},
        {CapRole, "cap"},
        {CodecRole, "codec"},
        {CapsRole, "caps"},
        {CodecsRole, "codecs"},
    };
}

void CameraModel::add_camera(Camera *camera)
{
    beginInsertRows(QModelIndex(), _cameras.size(), _cameras.size());
    auto camera_item = new CameraItem(camera, this);
    _cameras.append(camera_item);
    endInsertRows();

    emit camera_count_changed();

    if (_cameras.size() == 1) {
        set_selected_camera_index(0);
    }
}

void CameraModel::remove_camera(const int index)
{
    if (index < 0 || index >= _cameras.size()) return;

    beginRemoveRows(QModelIndex(), index, index);
    _cameras.removeAt(index);
    endRemoveRows();

    emit camera_count_changed();

    if (_cameras.isEmpty()) {
        _selected_camera_index = -1;
        emit selected_camera_changed();
        return;
    }

    if (_selected_camera_index == index || _selected_camera_index >= _cameras.size()) {
        //     spdlog::info("remove_camera(), selected_camera_index = {}", _selected_camera_index);
        // _selected_camera_index = std::min(index, static_cast<int>(_cameras.size()) - 1);
        _selected_camera_index = 0;
        spdlog::info("remove_camera(), selected_camera_index = {}", _selected_camera_index);
        emit selected_camera_changed();
    }
}

int CameraModel::index_of_camera_id(const int id) const
{
    for (auto i = 0; i < _cameras.size(); ++i) {
        if (_cameras[i]->id() == id) return i;
    }
    return -1;
}

QVariantMap CameraModel::get(const int index) const
{
    QVariantMap map;
    if (index < 0 || index >= _cameras.size()) return map;

    const auto &camera = _cameras[index];
    spdlog::info(
        "get() id = {}, name = {}, cap = {}, codec = {}",
        camera->id(),
        camera->name().toStdString(),
        camera->cap().toStdString(),
        camera->codec().toStdString()
    );

    map["id"] = camera->id();
    map["camera_item"] = QVariant::fromValue(camera);
    map["cap"] = camera->cap();
    map["codec"] = camera->codec();

    return map;
}

bool CameraModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    auto row = index.row();
    if (!index.isValid() || row < 0 || row >= _cameras.size()) return false;

    auto camera = _cameras[row];
    spdlog::info(
        "setData() row = {}, role = {}, id = {}, name = {}, value = {}",
        row,
        role,
        camera->id(),
        camera->name().toStdString(),
        value.toString().toStdString()
    );
    // qDebug() << "setData()" << row << role << camera->id() << camera->name() <<
    // value.toString();
    // camera->caps() << camera->codecs() << value.toString();

    switch (role) {
    case NameRole: camera->set_name(value.toString()); break;
    case CapRole: camera->set_cap(value.toString()); break;
    case CodecRole: camera->set_codec(value.toString()); break;
    default: return false;
    }

    emit dataChanged(index, index, {role});
    return true;
}

void CameraModel::set_selected_camera_index(const int index)
{
    if (_selected_camera_index != index) {
        spdlog::info("set_selected_camera_index() = {}", index);
        _selected_camera_index = index;
        emit selected_camera_changed();
    }
}