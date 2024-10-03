#include <camera_item_widget.h>
#include <fmt/core.h>
#include <gst/app/gstappsink.h>
#include <gst/codecparsers/gsth265parser.h>
#include <gst/gstpipeline.h>
#include <gst/video/video-info.h>
#include <qnamespace.h>

#include <QDockwidget>
#include <QHBoxLayout>
#include <cmath>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "stream_mainwindow.h"
#include "stream_window.h"
#include "xdaq_camera_control.h"

CameraItemWidget::CameraItemWidget(Camera *_camera, QWidget *parent)
    : QWidget(parent), stream_window(nullptr)
{
    camera = _camera;
    QHBoxLayout *layout = new QHBoxLayout(this);
    name = new QCheckBox(QString::fromStdString(_camera->get_name()), this);
    resolution = new QComboBox(this);
    fps = new QComboBox(this);
    codec = new QComboBox(this);
    audio = new QCheckBox(tr("Audio"), this);

    // TODO: disable audio for now
    audio->setDisabled(true);

    layout->addWidget(name);
    layout->addWidget(resolution);
    layout->addWidget(fps);
    layout->addWidget(codec);
    layout->addWidget(audio);

    setLayout(layout);

    for (const auto &cap : camera->get_capabilities()) {
        parse(cap);
    }
    load_caps();

    connect(resolution, &QComboBox::currentTextChanged, [this](const QString &selected_resolution) {
        fps->clear();

        for (const auto &cap : capabilities) {
            if (cap.resolution == selected_resolution.toStdString()) {
                fps->addItem(QString::fromStdString(cap.fps));
            }
        }
        fps->setCurrentIndex(0);
    });
    connect(fps, &QComboBox::currentTextChanged, [this](const QString &selected_fps) {
        codec->clear();

        for (const auto &cap : capabilities) {
            if (cap.fps == selected_fps.toStdString()) {
                codec->addItem(QString::fromStdString(cap.codec));
            }
        }
        codec->setCurrentIndex(0);
    });
    connect(name, &QCheckBox::clicked, [this](bool checked) {
        XDAQCameraControl *xdaq_camera_control = qobject_cast<XDAQCameraControl *>(
            this->parentWidget()->parentWidget()->parentWidget()->parentWidget()
        );
        StreamMainWindow *stream_mainwindow = xdaq_camera_control->stream_mainwindow;

        if (checked) {
            camera->set_current_cap(dump());

            if (!stream_window) {
                stream_window = new StreamWindow(camera, stream_mainwindow);
                stream_mainwindow->addDockWidget(Qt::LeftDockWidgetArea, stream_window);
                stream_mainwindow->show();
                stream_window->play();
            }
        } else {
            stream_window->close();
            delete stream_window;
            stream_window = nullptr;

            if (stream_mainwindow->findChildren<StreamWindow *>().isEmpty()) {
                stream_mainwindow->close();
            } else {
                stream_mainwindow->adjustSize();
            }
        }
    });
}

void CameraItemWidget::parse(const std::string &capability)
{
    std::stringstream ss(capability);
    std::string token;
    std::unordered_map<std::string, std::string> m;
    std::string codec_str;

    // "video/x-raw,format=YUY2,width=640,height=480,framerate=30/1"
    while (std::getline(ss, token, ',')) {
        size_t pos = token.find('=');
        if (pos != std::string::npos) {
            std::string k = token.substr(0, pos);
            std::string v = token.substr(pos + 1);
            m[k] = v;
        } else {
            codec_str = token;
        }
    }

    display_gst[tr("format")] = m["format"];

    size_t framerate_pos = m["framerate"].find("/");
    if (framerate_pos != std::string::npos) {
        float frames = std::stof(m["framerate"].substr(0, framerate_pos));
        int second = std::stoi(m["framerate"].substr(framerate_pos + 1));

        auto framerate = frames / second;

        if (std::ceilf(framerate) == framerate) {
            m["fps"] = fmt::format("{}FPS", framerate);
            display_gst[QString::fromStdString(fmt::format("{}FPS", framerate))] = m["framerate"];
        } else {
            m["fps"] = fmt::format("{:.1f}FPS", framerate);
            display_gst[QString::fromStdString(fmt::format("{:.1f}FPS", framerate))] =
                m["framerate"];
        }
    }

    if (m["width"] == "176" && m["height"] == "144") {
        m["resolution"] = "144p";
        display_gst_resolution[tr("144p")] = std::make_pair(m["width"], m["height"]);
    } else if (m["width"] == "320" && m["height"] == "240") {
        m["resolution"] = "240p";
        display_gst_resolution[tr("240p")] = std::make_pair(m["width"], m["height"]);
    } else if (m["width"] == "480" && m["height"] == "360") {
        m["resolution"] = "360p";
        display_gst_resolution[tr("360p")] = std::make_pair(m["width"], m["height"]);
    } else if (m["width"] == "640" && m["height"] == "480") {
        m["resolution"] = "480p";
        display_gst_resolution[tr("480p")] = std::make_pair(m["width"], m["height"]);
    } else if (m["width"] == "1280" && m["height"] == "720") {
        m["resolution"] = "HD";
        display_gst_resolution[tr("HD")] = std::make_pair(m["width"], m["height"]);
    } else if (m["width"] == "1920" && m["height"] == "1080") {
        m["resolution"] = "Full HD";
        display_gst_resolution[tr("Full HD")] = std::make_pair(m["width"], m["height"]);
    } else if (m["width"] == "2048" && m["height"] == "1080") {
        m["resolution"] = "2K";
        display_gst_resolution[tr("2K")] = std::make_pair(m["width"], m["height"]);
    } else {
        m["resolution"] = fmt::format("{}x{}", m["width"], m["height"]);
        display_gst_resolution[QString::fromStdString(fmt::format("{}x{}", m["width"], m["height"])
        )] = std::make_pair(m["width"], m["height"]);
    }

    if (codec_str == "video/x-raw") {
        codec_str = "H.265";
        display_gst[tr("H.265")] = "video/x-raw";
    } else if (codec_str == "image/jpeg") {
        codec_str = "JPEG";
        display_gst[tr("JPEG")] = "image/jpeg";
    } else {
        codec_str = "Unprocessable Codec";
        display_gst[tr("Unprocessable Codec")] = "...";
    }

    Capability cap{
        .codec = codec_str,
        .format = m["format"],
        // .width = m["width"],
        // .height = m["height"],
        .resolution = m["resolution"],
        .fps = m["fps"]
        // .framerate = m["framerate"]
    };
    capabilities.emplace_back(cap);

    codec_set.insert(codec_str);
    resolution_set.insert(m["resolution"]);
    fps_set.insert(m["fps"]);
}

std::string CameraItemWidget::dump()
{
    auto [w, h] = display_gst_resolution[resolution->currentText()];
    std::string f = display_gst[fps->currentText()];
    std::string c = display_gst[codec->currentText()];

    return fmt::format(
        "{},format={},width={},height={},framerate={}", c, display_gst[tr("format")], w, h, f
    );
}

void CameraItemWidget::load_caps()
{
    codec->clear();
    resolution->clear();
    fps->clear();
    for (const auto &c : codec_set) {
        codec->addItem(QString::fromStdString(c));
    }
    for (const auto &r : resolution_set) {
        resolution->addItem(QString::fromStdString(r));
    }
    for (const auto &f : fps_set) {
        fps->addItem(QString::fromStdString(f));
    }
    resolution->setCurrentIndex(0);
    fps->setCurrentIndex(0);
    codec->setCurrentIndex(0);
}

void CameraItemWidget::play()
{
    XDAQCameraControl *xdaq_camera_control = qobject_cast<XDAQCameraControl *>(
        this->parentWidget()->parentWidget()->parentWidget()->parentWidget()
    );
    StreamMainWindow *stream_mainwindow = xdaq_camera_control->stream_mainwindow;
    // QString dockwidget_name = QString("%1").arg(name->text());

    // StreamWindow *existing_window = stream_mainwindow->findChild<StreamWindow
    // *>(dockwidget_name); stream_mainwindow->find();

    // if (existing_dock) {
    //     stream_window->play();
    //     existing_dock->raise();
    //     // existing_dock->activateWindow();
    //     return;
    // } else {
    stream_window = new StreamWindow(camera, stream_mainwindow);
    stream_mainwindow->addDockWidget(Qt::LeftDockWidgetArea, stream_window);
    stream_mainwindow->show();
    stream_window->play();
    // }
}

// void CameraItemWidget::pause() { stream_window->pause(); }