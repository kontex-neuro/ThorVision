#include "CameraModel.h"

#include <spdlog/spdlog.h>

GstPadProbeReturn extract_metadata(
    [[maybe_unused]] GstPad *pad, GstPadProbeInfo *info, gpointer user_data
)
{
    auto buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (!buffer) {
        spdlog::error("Failed to get buffer");
        return GST_PAD_PROBE_DROP;
    }
    auto camera_item = static_cast<CameraItem *>(user_data);

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        spdlog::error("Failed to read buffer");
        return GST_PAD_PROBE_DROP;
    }
    gst_buffer_unmap(buffer, &map);

    auto xdaqmetadata = camera_item->_stream->_metadata_handler->safe_deque.check_pts_pop_timestamp(
        GST_BUFFER_PTS(buffer)
    );
    auto metadata = xdaqmetadata.value_or(XDAQFrameData{0, 0, 0, 0, 0, 0});
    camera_item->update_metadata(metadata);

    return GST_PAD_PROBE_OK;
}

auto add_stream(QQuickItem *video_item, int index, int port)
    -> std::optional<std::unique_ptr<Stream>>
{
    qDebug() << "Port:" << port << "video_item:" << video_item << "index:" << index;

    if (!video_item) {
        spdlog::error("video_item is null");
        return std::nullopt;
    }

    auto uri = fmt::format("{}:{}", "192.168.177.100", port);
    auto pipeline = gst_pipeline_new(nullptr);
    gst_element_set_start_time(pipeline, GST_CLOCK_TIME_NONE);

    auto src = gst_element_factory_make("srtclientsrc", "src");
    auto parser = gst_element_factory_make("jpegparse", "parser");
    auto tee = gst_element_factory_make("tee", "t");
    auto queue_display = gst_element_factory_make("queue", "queue_display");
#ifdef _WIN32
    auto dec = gst_element_factory_make("jpegdec", "dec");
#elif __APPLE__
    auto dec = gst_element_factory_make("vtdec", "dec");
#else
    auto dec = gst_element_factory_make("jpegdec", "dec");
#endif
    auto conv = gst_element_factory_make("videoconvert", "conv");
    auto cf_conv = gst_element_factory_make("capsfilter", "cf_conv");
    auto glupload = gst_element_factory_make("glupload", "glupload");
    auto sink = gst_element_factory_make("qml6glsink", "sink");
    auto fpsdisplaysink = gst_element_factory_make("fpsdisplaysink", "fpsdisplaysink");

    if (!src || !parser || !tee || !queue_display || !dec || !conv || !cf_conv || !glupload ||
        !sink) {
        fmt::print(stderr, "Failed to create elements.\n");
        return std::nullopt;
    }

    // clang-format off
    std::unique_ptr<GstCaps, decltype(&gst_caps_unref)> cf_conv_caps(
        gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "RGB",
        nullptr),
        gst_caps_unref
    );
    // clang-format on

    g_object_set(src, "uri", fmt::format("srt://{}", uri).c_str(), nullptr);
    g_object_set(cf_conv, "caps", cf_conv_caps.get(), nullptr);
    g_object_set(sink, "sync", false, nullptr);
    g_object_set(fpsdisplaysink, "video-sink", sink, nullptr);
    g_object_set(fpsdisplaysink, "text-overlay", false, nullptr);
    g_object_set(fpsdisplaysink, "sync", false, nullptr);
    g_object_set(sink, "widget", video_item, nullptr);

    gst_bin_add_many(
        GST_BIN(pipeline),
        src,
        parser,
        tee,
        queue_display,
        dec,
        conv,
        cf_conv,
        glupload,
        fpsdisplaysink,
        nullptr
    );

    if (!gst_element_link_many(src, parser, tee, nullptr) ||
        !gst_element_link_many(
            tee, queue_display, dec, conv, cf_conv, glupload, fpsdisplaysink, nullptr
        )) {
        spdlog::error("Elements could not be linked.");
        gst_object_unref(pipeline);
        return std::nullopt;
    }

    auto stream = std::make_unique<Stream>(GST_PIPELINE(pipeline), index);

    auto src_pad = gst_element_get_static_pad(parser, "src");
    if (!src_pad) {
        spdlog::error("Failed to get src pad from parser.");
        return std::nullopt;
    }
    gst_pad_add_probe(
        src_pad,
        GST_PAD_PROBE_TYPE_BUFFER,
        parse_jpeg_metadata,
        stream->_metadata_handler.get(),
        nullptr
    );
    gst_object_unref(src_pad);

    return std::move(stream);
}

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

void CameraModel::onItemAdded(int index, QQuickItem *item)
{
    auto camera = _cameras.at(index);
    if (!camera) return;

    auto video_item = item->findChild<QQuickItem *>("video_item");
    if (!video_item) return;

    if (auto stream = add_stream(video_item, index, camera->port())) {
        auto pipeline = (*stream)->_pipeline;
        auto sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
        auto sink_pad = gst_element_get_static_pad(sink, "sink");
        if (!sink_pad) {
            spdlog::error("Failed to get sink pad from sink.");
            return;
        }
        gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_BUFFER, extract_metadata, camera, nullptr);
        gst_object_unref(sink_pad);

        camera->_stream = std::move(*stream);
    }
}

// void CameraModel::onItemRemoved(int index, QQuickItem *item)
// {
//     // auto camera = _cameras.at(index);
//     // if (!camera) return;
//     // camera->_stream.reset();
// }