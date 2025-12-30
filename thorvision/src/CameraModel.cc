#include "CameraModel.h"

#include <spdlog/spdlog.h>

#include "Stream.h"

auto add_stream(QQuickItem *video_item, int index, int port)
    -> std::optional<std::unique_ptr<Stream>>
{
    spdlog::info("add_stream() index = {}, port = {}", index, port);

    if (!video_item) {
        spdlog::error("video_item is null");
        return std::nullopt;
    }

    // TODO: default create image/jpeg pipeline
    auto stream = std::make_unique<Stream>(
        Stream::pipeline(fmt::format("{}:{}", "192.168.177.100", port), "image/jpeg"),
        video_item,
        index,
        port
    );

    return std::move(stream);
}

CameraModel::CameraModel(QObject *parent) : QAbstractListModel(parent)
{
    _selected_camera_index = -1;
}

CameraModel::~CameraModel()
{
    for (auto cam : _cameras) {
        spdlog::info("Deleting camera id = {}, name = {}", cam->id(), cam->name().toStdString());
        cam->deleteLater();
    }
    qDeleteAll(_cameras);
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
    auto camera_item = new CameraItem(camera, this);

    beginInsertRows(QModelIndex(), _cameras.size(), _cameras.size());
    _cameras.append(camera_item);
    endInsertRows();

    connect(camera_item, &CameraItem::stream_status_changed, this, [this]() {
        emit all_cams_streaming();
    });

    emit camera_item->stream_status_changed(false);
    emit camera_count_changed();

    if (_cameras.size() == 1) {
        set_selected_camera_index(0);
    }
}

void CameraModel::remove_camera(const int index)
{
    if (index < 0 || index >= _cameras.size()) return;

    beginRemoveRows(QModelIndex(), index, index);
    _cameras.at(index)->deleteLater();
    _cameras.removeAt(index);
    endRemoveRows();

    spdlog::info("Removed camera at index {}, {}", index, _cameras.size());

    for (auto camera : _cameras) {
        spdlog::info(
            "Remaining camera id = {}, name = {}", camera->id(), camera->name().toStdString()
        );
    }

    emit camera_count_changed();

    if (_cameras.isEmpty()) {
        _selected_camera_index = -1;
        emit selected_camera_changed();
        return;
    }

    if (_selected_camera_index == index || _selected_camera_index >= _cameras.size()) {
        //     spdlog::info("remove_camera(), selected_camera_index = {}",
        //     _selected_camera_index);
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
    spdlog::debug(
        "get() id = {}, name = {}, cap = {}, codec = {}",
        camera->id(),
        camera->name().toStdString(),
        camera->cap().toStdString(),
        camera->codec().toStdString()
    );

    map["id"] = camera->id();
    map["camera_item"] = QVariant::fromValue(camera);
    map["device_id"] = camera->device_id();
    map["name"] = camera->name();
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

void CameraModel::onItemAdded(int index, QQuickItem *item)
{
    auto camera = _cameras.at(index);

    auto loader = item->findChild<QQuickItem *>("loader");
    assert(loader != nullptr && "[qml] Could not find loader");

    auto video_item = loader->property("item").value<QQuickItem *>();
    assert(video_item != nullptr && "[qml] Could not find GstVideoItem");

    if (auto stream = add_stream(video_item, index, camera->port())) {
        camera->set_stream(std::move(*stream));
    }
}

bool CameraModel::all_cameras_streaming() const
{
    for (const auto &camera : _cameras) {
        if (camera && !camera->is_streaming()) {
            return false;
        }
    }
    return rowCount() == 0 ? false : true;
}