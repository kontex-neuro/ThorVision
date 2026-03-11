#include "CameraModel.h"

#include <spdlog/spdlog.h>

#include "Stream.h"

CameraModel::CameraModel(QObject *parent) : QAbstractListModel(parent), _selected_camera_index(-1)
{
}

CameraModel::~CameraModel()
{
    qDeleteAll(_cameras);
    _cameras.clear();
}

int CameraModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
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
    spdlog::debug("CameraModel::roleNames()");
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

void CameraModel::add_camera(std::unique_ptr<Camera> camera)
{
    const auto &name = QString::fromStdString(camera->name());
    auto camera_item = new CameraItem(std::move(camera));
    camera_item->set_name(unique_camera_name(name, _cameras.size()));

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

    camera_item->set_cap(camera_item->default_cap());
    camera_item->set_codec(camera_item->default_codec());
}

void CameraModel::remove_camera(const int index)
{
    if (index < 0 || index >= _cameras.size()) return;

    beginRemoveRows(QModelIndex(), index, index);
    auto camera_item = _cameras.takeAt(index);
    // _cameras.removeAt(index);
    camera_item->deleteLater();
    endRemoveRows();

    spdlog::info("Removed camera at index {}, ({} remaining)", index, _cameras.size());

    emit camera_count_changed();

    if (_cameras.isEmpty()) {
        set_selected_camera_index(-1);
    } else if (_selected_camera_index == index || _selected_camera_index >= _cameras.size()) {
        set_selected_camera_index(0);
    }
}

int CameraModel::index_of_camera_id(const int id) const noexcept
{
    for (auto i = 0; i < _cameras.size(); ++i) {
        if (_cameras[i]->id() == id) return i;
    }
    return -1;
}

QVariantMap CameraModel::get(const int index) const noexcept
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

void CameraModel::onItemAdded(int index, QQuickItem *item)
{
    auto camera = _cameras.at(index);

    auto loader = item->findChild<QQuickItem *>("loader");
    assert(loader && "[qml] Could not find loader");

    auto video_item = loader->property("item").value<QQuickItem *>();
    assert(video_item && "[qml] Could not find GstVideoItem");

    if (auto stream = std::make_unique<Stream>(video_item, index, camera->port())) {
        camera->set_stream(std::move(stream));
    }
}

void CameraModel::onItemRemoved(int index, QQuickItem *)
{
    spdlog::info("CameraModel::onItemRemoved index = {}", index);
}

bool CameraModel::all_cameras_streaming() const
{
    if (_cameras.isEmpty()) return false;
    for (const auto &camera : _cameras) {
        if (camera && !camera->streaming()) {
            return false;
        }
    }
    return true;
}

bool CameraModel::set_name(int index, const QString &value) const
{
    auto camera = _cameras.at(index);
    const auto &name = unique_camera_name(value, index);
    if (name == camera->name()) return false;
    camera->set_name(name);
    return true;
}

QString CameraModel::unique_camera_name(const QString &base, int self_index) const
{
    auto name = base.trimmed();
    if (name.isEmpty()) name = "Camera";

    QSet<QString> used;
    for (auto i = 0; i < _cameras.size(); ++i) {
        if (i == self_index) continue;
        used.insert(_cameras[i]->name());
    }

    if (!used.contains(name)) return name;

    auto n = 1;
    QString candidate;
    do {
        candidate = QString("%1 (%2)").arg(name).arg(n++);
    } while (used.contains(candidate));

    return candidate;
}