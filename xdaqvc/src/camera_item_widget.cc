#include "camera_item_widget.h"

#include <fmt/core.h>
#include <gst/app/gstappsink.h>
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

#include "xdaq_camera_control.h"


CameraItemWidget::CameraItemWidget(Camera *_camera, QWidget *parent)
    : QWidget(parent), stream_window(nullptr)
{
    camera = _camera;
    auto layout = new QHBoxLayout(this);
    auto name = new QCheckBox(QString::fromStdString(_camera->name()), this);
    auto view = new QRadioButton(tr("View"), this);
    auto resolution = new QComboBox(this);
    auto fps = new QComboBox(this);
    auto codec = new QComboBox(this);
    auto audio = new QCheckBox(tr("Audio"), this);

    resolution->addItem("");
    fps->addItem("");
    codec->addItem("");

    QColor valid_selection(163, 139, 81);
    QColor invalid_selection(102, 102, 102);

    auto valid_style = QString("rgb(%1, %2, %3)")
                           .arg(valid_selection.red())
                           .arg(valid_selection.green())
                           .arg(valid_selection.blue());

    resolution->setStyleSheet(QString("QComboBox { color: %1; }").arg(valid_style));
    fps->setStyleSheet(QString("QComboBox { color: %1; }").arg(valid_style));
    codec->setStyleSheet(QString("QComboBox { color: %1; }").arg(valid_style));

    name->setDisabled(true);
    view->setChecked(true);
    // TODO: disable audio for now
    audio->setDisabled(true);

    layout->addWidget(name);
    layout->addWidget(resolution);
    layout->addWidget(fps);
    layout->addWidget(codec);
    layout->addWidget(view);
    layout->addWidget(audio);
    setLayout(layout);

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

    for (const auto &cap : camera->caps()) {
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

        auto _fps = (float) cap.fps_n / cap.fps_d;
        if (std::ceilf(_fps) == _fps) {
            cap_text.fps = std::make_pair(
                fmt::format("{}/{}", cap.fps_n, cap.fps_d),
                QString::fromStdString(fmt::format("{} FPS", _fps))
            );
        } else {
            cap_text.fps = std::make_pair(
                fmt::format("{}/{}", cap.fps_n, cap.fps_d),
                QString::fromStdString(fmt::format("{:.2f} FPS", _fps))
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
        if (codec->findText(cap.codec.second) == -1) codec->addItem(cap.codec.second);
        if (resolution->findText(cap.resolution.second) == -1)
            resolution->addItem(cap.resolution.second);
        if (fps->findText(cap.fps.second) == -1) fps->addItem(cap.fps.second);
    }

    for (int i = 0; i < resolution->count(); ++i)
        resolution->setItemData(i, QBrush(valid_selection), Qt::ForegroundRole);
    for (int i = 0; i < fps->count(); ++i)
        fps->setItemData(i, QBrush(valid_selection), Qt::ForegroundRole);
    for (int i = 0; i < codec->count(); ++i)
        codec->setItemData(i, QBrush(valid_selection), Qt::ForegroundRole);

    auto check_name_clickable = [name, resolution, fps, codec]() {
        name->setEnabled(
            !resolution->currentText().isEmpty() && !fps->currentText().isEmpty() &&
            !codec->currentText().isEmpty()
        );
    };

    auto reset_items = [invalid_selection](QComboBox *combobox) {
        for (int i = 0; i < combobox->count(); ++i)
            combobox->setItemData(i, QBrush(invalid_selection), Qt::ForegroundRole);
    };

    auto highlight_items =
        [valid_selection](QComboBox *combobox, const QString &text, bool condition) {
            if (condition) {
                int index = combobox->findText(text);
                combobox->setItemData(index, QBrush(valid_selection), Qt::ForegroundRole);
            }
        };

    connect(resolution, &QComboBox::currentIndexChanged, [=, this](int index) {
        if (resolution->itemText(index).isEmpty()) {
            name->setEnabled(false);
            return;
        }
        check_name_clickable();

        if (resolution->itemData(index, Qt::ForegroundRole).value<QColor>() == invalid_selection) {
            fps->setCurrentIndex(0);
            codec->setCurrentIndex(0);
        }

        reset_items(resolution);
        reset_items(fps);
        reset_items(codec);

        auto current_res = resolution->currentText();
        auto current_fps = fps->currentText();
        auto current_codec = codec->currentText();

        if (current_fps.isEmpty() && current_codec.isEmpty()) {
            for (int i = 0; i < resolution->count(); ++i)
                resolution->setItemData(i, QBrush(valid_selection), Qt::ForegroundRole);
        }
        for (const auto &cap : caps) {
            auto [_r, res_text] = cap.resolution;
            auto [_f, fps_text] = cap.fps;
            auto [_c, codec_text] = cap.codec;

            auto res_match = current_res.isEmpty() || res_text == current_res;
            auto fps_match = current_fps.isEmpty() || fps_text == current_fps;
            auto codec_match = current_codec.isEmpty() || codec_text == current_codec;

            if (current_fps.isEmpty() && current_codec.isEmpty()) {
                if (res_match) {
                    highlight_items(fps, fps_text, true);
                    highlight_items(codec, codec_text, true);
                }
            } else {
                highlight_items(resolution, res_text, fps_match && codec_match);
                highlight_items(fps, fps_text, res_match && codec_match);
                highlight_items(codec, codec_text, res_match && fps_match);
            }
        }
    });
    connect(fps, &QComboBox::currentIndexChanged, [=, this](int index) {
        if (fps->itemText(index).isEmpty()) {
            name->setEnabled(false);
            return;
        }
        check_name_clickable();

        if (fps->itemData(index, Qt::ForegroundRole).value<QColor>() == invalid_selection) {
            resolution->setCurrentIndex(0);
            codec->setCurrentIndex(0);
        }

        reset_items(resolution);
        reset_items(fps);
        reset_items(codec);

        auto current_res = resolution->currentText();
        auto current_fps = fps->currentText();
        auto current_codec = codec->currentText();

        if (current_res.isEmpty() && current_codec.isEmpty()) {
            for (int i = 0; i < fps->count(); ++i)
                fps->setItemData(i, QBrush(valid_selection), Qt::ForegroundRole);
        }

        for (const auto &cap : caps) {
            auto [_r, res_text] = cap.resolution;
            auto [_f, fps_text] = cap.fps;
            auto [_c, codec_text] = cap.codec;

            auto res_match = current_res.isEmpty() || res_text == current_res;
            auto fps_match = current_fps.isEmpty() || fps_text == current_fps;
            auto codec_match = current_codec.isEmpty() || codec_text == current_codec;

            if (current_res.isEmpty() && current_codec.isEmpty()) {
                if (fps_match) {
                    highlight_items(resolution, res_text, true);
                    highlight_items(codec, codec_text, true);
                }
            } else {
                highlight_items(resolution, res_text, fps_match && codec_match);
                highlight_items(fps, fps_text, res_match && codec_match);
                highlight_items(codec, codec_text, res_match && fps_match);
            }
        }
    });
    connect(codec, &QComboBox::currentIndexChanged, [=, this](int index) {
        if (codec->itemText(index).isEmpty()) {
            name->setEnabled(false);
            return;
        }
        check_name_clickable();

        if (codec->itemData(index, Qt::ForegroundRole).value<QColor>() == invalid_selection) {
            resolution->setCurrentIndex(0);
            fps->setCurrentIndex(0);
        }

        reset_items(resolution);
        reset_items(fps);
        reset_items(codec);

        auto current_res = resolution->currentText();
        auto current_fps = fps->currentText();
        auto current_codec = codec->currentText();

        if (current_res.isEmpty() && current_fps.isEmpty()) {
            for (int i = 0; i < codec->count(); ++i)
                codec->setItemData(i, QBrush(valid_selection), Qt::ForegroundRole);
        }

        for (const auto &cap : caps) {
            auto [_r, res_text] = cap.resolution;
            auto [_f, fps_text] = cap.fps;
            auto [_c, codec_text] = cap.codec;

            auto res_match = current_res.isEmpty() || res_text == current_res;
            auto fps_match = current_fps.isEmpty() || fps_text == current_fps;
            auto codec_match = current_codec.isEmpty() || codec_text == current_codec;

            if (current_res.isEmpty() && current_fps.isEmpty()) {
                if (codec_match) {
                    highlight_items(resolution, res_text, true);
                    highlight_items(fps, fps_text, true);
                }
            } else {
                highlight_items(resolution, res_text, fps_match && codec_match);
                highlight_items(fps, fps_text, res_match && codec_match);
                highlight_items(codec, codec_text, res_match && fps_match);
            }
        }
    });
    connect(name, &QCheckBox::clicked, [this, resolution, fps, codec](bool checked) {
        // HACK
        auto main_window = qobject_cast<XDAQCameraControl *>(
            parentWidget()->parentWidget()->parentWidget()->parentWidget()
        );
        auto stream_mainwindow = main_window->stream_mainwindow;

        if (checked) {
            std::string raw_cap;
            for (const auto &cap : caps) {
                auto [r, r_text] = cap.resolution;
                auto [f, f_text] = cap.fps;
                auto [c, c_text] = cap.codec;

                if (r_text == resolution->currentText() && f_text == fps->currentText() &&
                    c_text == codec->currentText()) {
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

            auto area = stream_mainwindow->findChildren<StreamWindow *>().size() < 2
                            ? Qt::LeftDockWidgetArea
                            : Qt::TopDockWidgetArea;
            if (!stream_window) {
                stream_window = new StreamWindow(camera, stream_mainwindow);
                stream_mainwindow->addDockWidget(area, stream_window);
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
    connect(view, &QRadioButton::toggled, [this](bool checked) {
        if (stream_window) {
            if (checked) {
                stream_window->show();
            } else {
                stream_window->hide();
            }
        }
    });
}