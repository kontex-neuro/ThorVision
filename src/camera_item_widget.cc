#include <camera_item_widget.h>
#include <fmt/core.h>
#include <gst/app/gstappsink.h>
#define GST_USE_UNSTABLE_API
#include <gst/codecparsers/gsth265parser.h>
#include <gst/gstpipeline.h>
#include <gst/video/video-info.h>
#include <qnamespace.h>
#include <spdlog/spdlog.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDockwidget>
#include <QHBoxLayout>
#include <QRadioButton>
#include <cmath>
#include <string>
#include <tuple>

#include "stream_mainwindow.h"
#include "stream_window.h"
#include "xdaq_camera_control.h"


CameraItemWidget::CameraItemWidget(Camera *_camera, QWidget *parent)
    : QWidget(parent), stream_window(nullptr)
{
    camera = _camera;
    QHBoxLayout *layout = new QHBoxLayout(this);
    QCheckBox *name = new QCheckBox(QString::fromStdString(_camera->get_name()), this);
    QRadioButton *view = new QRadioButton(tr("View"), this);
    QComboBox *resolution = new QComboBox(this);
    QComboBox *fps = new QComboBox(this);
    QComboBox *codec = new QComboBox(this);
    QCheckBox *audio = new QCheckBox(tr("Audio"), this);

    view->setChecked(false);
    // TODO: disable audio for now
    audio->setDisabled(true);

    layout->addWidget(name);
    layout->addWidget(resolution);
    layout->addWidget(fps);
    layout->addWidget(codec);
    layout->addWidget(view);
    layout->addWidget(audio);
    setLayout(layout);

    // R: 181 G: 157 B: 99
    const std::map<Resolution, QString> rm = {
        {{176, 144}, tr("144p")},
        {{320, 240}, tr("240p")},
        {{480, 360}, tr("360p")},
        {{640, 480}, tr("480p")},
        {{1280, 720}, tr("HD")},
        {{1920, 1080}, tr("Full HD")},
        {{2048, 1080}, tr(" 2K ")}
    };

    const std::map<std::string, QString> cm = {
        {"video/x-raw", tr("H.265")},
        {"image/jpeg", tr("JPEG")},
    };

    for (const auto &cap : camera->get_caps()) {
        CapText cap_text;

        Resolution r{cap.width, cap.height};
        auto find_resolution = [rm](const Resolution &res) -> QString {
            auto it = rm.find(res);
            return it != rm.end()
                       ? it->second
                       : QString::fromStdString(fmt::format("{}x{}", res.width, res.height));
        };
        cap_text.resolution = std::make_pair(r, find_resolution(r));

        if (cap.media_type == "video/x-raw") {
            cap_text.format = cap.format;
        }

        auto fps = (float) cap.fps_n / cap.fps_d;
        if (std::ceilf(fps) == fps) {
            cap_text.fps = std::make_pair(
                fmt::format("{}/{}", cap.fps_n, cap.fps_d),
                QString::fromStdString(fmt::format("{} FPS", fps))
            );
        } else {
            cap_text.fps = std::make_pair(
                fmt::format("{}/{}", cap.fps_n, cap.fps_d),
                QString::fromStdString(fmt::format("{:.2f} FPS", fps))
            );
        }

        auto find_codec = [cm](const std::string &codec) -> QString {
            auto it = cm.find(codec);
            return it != cm.end() ? it->second : QString::fromStdString(fmt::format("{}", codec));
        };
        cap_text.codec = std::make_pair(cap.media_type, find_codec(cap.media_type));

        caps.emplace_back(cap_text);
    }

    for (const auto &cap : caps) {
        if (codec->findText(cap.codec.second) == -1) {
            codec->addItem(cap.codec.second);
        }
        if (resolution->findText(cap.resolution.second) == -1) {
            resolution->addItem(cap.resolution.second);
        }
        if (fps->findText(cap.fps.second) == -1) {
            fps->addItem(cap.fps.second);
        }
    }

    connect(
        resolution,
        &QComboBox::currentTextChanged,
        [this, fps, codec](const QString &selected_resolution) {
            spdlog::info("change resolution");
            fps->clear();
            codec->clear();

            for (const auto &cap : caps) {
                auto [_r, resolution_text] = cap.resolution;
                if (selected_resolution == resolution_text) {
                    auto [_f, fps_text] = cap.fps;
                    auto [_c, codec_text] = cap.codec;
                    if (fps->findText(fps_text) == -1) {
                        fps->addItem(fps_text);
                    }
                    if (codec->findText(codec_text) == -1) {
                        codec->addItem(codec_text);
                    }
                }
            }
            fps->setCurrentIndex(0);
            codec->setCurrentIndex(0);
        }
    );
    connect(
        fps,
        &QComboBox::currentTextChanged,
        [this, resolution, codec](const QString &selected_fps) {
            spdlog::info("change fps");
            codec->clear();

            for (const auto &cap : caps) {
                auto [_r, resolution_text] = cap.resolution;
                auto [_f, fps_text] = cap.fps;
                if (selected_fps == fps_text && resolution->currentText() == resolution_text) {
                    auto [_c, codec_text] = cap.codec;
                    if (codec->findText(codec_text) == -1) {
                        codec->addItem(codec_text);
                    }
                }
            }
            codec->setCurrentIndex(0);
        }
    );
    connect(name, &QCheckBox::clicked, [this, resolution, fps, codec, view](bool checked) {
        XDAQCameraControl *xdaq_camera_control = qobject_cast<XDAQCameraControl *>(
            this->parentWidget()->parentWidget()->parentWidget()->parentWidget()
        );
        StreamMainWindow *stream_mainwindow = xdaq_camera_control->stream_mainwindow;

        if (checked) {
            auto resolution_text = resolution->currentText();
            auto fps_text = fps->currentText();
            auto codec_text = codec->currentText();
            std::string raw_cap;

            for (const auto &cap : caps) {
                auto [r, r_text] = cap.resolution;
                auto [f, f_text] = cap.fps;
                auto [c, c_text] = cap.codec;

                if (r_text == resolution_text && f_text == fps_text && c_text == codec_text) {
                    if (c == "video/x-raw") {
                        raw_cap = fmt::format(
                            "{},format={},width={},height={},framerate={}",
                            c,
                            cap.format,
                            r.width,
                            r.height,
                            f
                        );
                    } else {
                        raw_cap = fmt::format(
                            "{},width={},height={},framerate={}", c, r.width, r.height, f
                        );
                    }
                }
            }
            camera->set_current_cap(raw_cap);

            if (!stream_window) {
                stream_window = new StreamWindow(camera, stream_mainwindow);
                stream_mainwindow->addDockWidget(Qt::LeftDockWidgetArea, stream_window);
                if (view->isChecked()) {
                    stream_mainwindow->show();
                }
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