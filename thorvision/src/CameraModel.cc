#include "CameraModel.h"

#include <spdlog/spdlog.h>

CameraModel::CameraModel(QObject *parent) : QAbstractListModel(parent)
{
    // _selected_camera = nullptr;
    _selected_camera_index = -1;
}

CameraModel::~CameraModel()
{
    // qDeleteAll(_cameras);
    // _cameras.clear();
    // _selected_camera->deleteLater();
}

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
    // qDebug() << "data()" << row << role << camera->id() << camera->name() << camera->cap()
    //          << camera->codec();
    // << camera->caps()
    //          << camera->codecs();

    switch (role) {
    case IdRole: return camera->id();
    case NameRole: return camera->name().isEmpty() ? QVariant() : camera->name();
    case CapsRole: return camera->caps().isEmpty() ? QVariant() : camera->caps();
    case CodecsRole: return camera->codecs().isEmpty() ? QVariant() : camera->codecs();
    case CapRole: return camera->cap().isEmpty() ? QVariant() : camera->cap();
    case CodecRole: return camera->codec().isEmpty() ? QVariant() : camera->codec();
    // case CameraItemRole: return QVariant::fromValue(camera);
    default: return QVariant();
    }
}

QHash<int, QByteArray> CameraModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {CapsRole, "caps"},
        {CodecsRole, "codecs"},
        {CapRole, "cap"},
        {CodecRole, "codec"},
        // {CameraItemRole, "camera_item"},
    };
}

void CameraModel::add_camera(Camera *camera, ImageProvider *provider)
{
    beginInsertRows(QModelIndex(), _cameras.size(), _cameras.size());
    auto camera_item = new CameraItem(camera, provider);
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
    auto item = _cameras[index];
    _cameras.removeAt(index);
    endRemoveRows();

    item->deleteLater();
    emit camera_count_changed();

    // if (_selected_camera == _cameras[index]) {
    //     _selected_camera = nullptr;
    //     emit selected_camera_changed();
    // }

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

void CameraModel::set_name(const int index, const QString &name)
{
    spdlog::info("set_name() index = {}, name = {}", index, name.toStdString());
    setData(this->index(index), name, NameRole);
}

void CameraModel::set_cap(const int index, const QString &cap)
{
    spdlog::info("set_cap() index = {}, cap = {}", index, cap.toStdString());
    setData(this->index(index), cap, CapRole);
}

void CameraModel::set_codec(const int index, const QString &codec)
{
    spdlog::info("set_codec() index = {}, codec = {}", index, codec.toStdString());
    setData(this->index(index), codec, CodecRole);
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
    // qDebug() << "get()" << camera->id() << camera->name() << camera->cap() << camera->codec();
    // camera->caps() << camera->codecs();

    map["id"] = camera->id();
    map["name"] = camera->name();
    map["caps"] = QVariant::fromValue(camera->caps().toList());
    map["codecs"] = QVariant::fromValue(camera->codecs().toList());
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
    // qDebug() << "setData()" << row << role << camera->id() << camera->name() << value.toString();
    // camera->caps()
    //          << camera->codecs() << value.toString();

    switch (role) {
    case NameRole: camera->set_name(value.toString()); break;
    case CapRole: camera->set_cap(value.toString()); break;
    case CodecRole: camera->set_codec(value.toString()); break;
    default: return false;
    }

    emit dataChanged(index, index, {role});
    return true;
}

CameraItem *CameraModel::selected_camera() const
{
    // spdlog::info("selected_camera() = {}", _selected_camera->id());
    // return _selected_camera;
    if (_selected_camera_index < 0 || _selected_camera_index >= _cameras.size()) return nullptr;

    spdlog::info("selected_camera(), id = {}", _cameras[_selected_camera_index]->id());
    return _cameras[_selected_camera_index];
}

// void CameraModel::set_selected_camera(CameraItem *camera)
// {
//     spdlog::info("set_selected_camera() = {}", camera->id());

//     _selected_camera = camera;

//     emit selected_camera_changed();

//     // if (_selected_camera_index != index) {
//     //     _selected_camera_index = index;
//     //     spdlog::info("set_selected_camera() = {}", _selected_camera_index);
//     //     // _selected_camera = _cameras[_selected_camera_indexp];
//     //     emit selected_camera_changed();
//     // }
// }

int CameraModel::selected_camera_index() const
{
    // if (_selected_camera == nullptr) return -1;
    // return _cameras.indexOf(_selected_camera);

    spdlog::info("selected_camera_index() = {}", _selected_camera_index);
    return _selected_camera_index;
}

void CameraModel::set_selected_camera_index(const int index)
{
    if (_selected_camera_index != index) {
        _selected_camera_index = index;
        spdlog::info("set_selected_camera_index() = {}", _selected_camera_index);
        _selected_camera = _cameras[_selected_camera_index];
        emit selected_camera_changed();
    }
}