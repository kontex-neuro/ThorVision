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
#include <string>

#include "xdaq_camera_control.h"


namespace
{
auto constexpr VIDEO_RAW = "video/x-raw";
auto constexpr VIDEO_MJPEG = "image/jpeg";
}  // namespace


CameraItemWidget::CameraItemWidget(Camera *camera, QWidget *parent)
    : QWidget(parent), _stream_window(nullptr)
{
    auto layout = new QHBoxLayout(this);
    auto name = new QCheckBox(QString::fromStdString(camera->name()), this);
    auto resolution = new QComboBox(this);
    auto fps = new QComboBox(this);
    auto codec = new QComboBox(this);
    auto view = new QRadioButton(tr("View"), this);
    auto audio = new QCheckBox(tr("Audio"), this);

    resolution->addItem("");
    fps->addItem("");
    codec->addItem("");

    QColor valid_selection(0, 0, 0);
    QColor invalid_selection(129, 140, 141);

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

    // TODO: Find a more flexible way to determine when to use that codec
    const std::map<std::string, QString> cm = {
        // {VIDEO_RAW, tr("H.265")},
        {VIDEO_RAW, tr("raw -> M-JPEG")},
        {VIDEO_MJPEG, tr("M-JPEG")},
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

        if (cap.media_type == VIDEO_RAW) {
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

        _caps.emplace_back(cap_text);
    }

    for (const auto &cap : _caps) {
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
        for (const auto &cap : _caps) {
            auto qt_r = cap.resolution.second;
            auto qt_f = cap.fps.second;
            auto qt_c = cap.codec.second;

            auto res_match = current_res.isEmpty() || qt_r == current_res;
            auto fps_match = current_fps.isEmpty() || qt_f == current_fps;
            auto codec_match = current_codec.isEmpty() || qt_c == current_codec;

            if (current_fps.isEmpty() && current_codec.isEmpty()) {
                if (res_match) {
                    highlight_items(fps, qt_f, true);
                    highlight_items(codec, qt_c, true);
                }
            } else {
                highlight_items(resolution, qt_r, fps_match && codec_match);
                highlight_items(fps, qt_f, res_match && codec_match);
                highlight_items(codec, qt_c, res_match && fps_match);
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

        for (const auto &cap : _caps) {
            auto qt_r = cap.resolution.second;
            auto qt_f = cap.fps.second;
            auto qt_c = cap.codec.second;

            auto res_match = current_res.isEmpty() || qt_r == current_res;
            auto fps_match = current_fps.isEmpty() || qt_f == current_fps;
            auto codec_match = current_codec.isEmpty() || qt_c == current_codec;

            if (current_res.isEmpty() && current_codec.isEmpty()) {
                if (fps_match) {
                    highlight_items(resolution, qt_r, true);
                    highlight_items(codec, qt_c, true);
                }
            } else {
                highlight_items(resolution, qt_r, fps_match && codec_match);
                highlight_items(fps, qt_f, res_match && codec_match);
                highlight_items(codec, qt_c, res_match && fps_match);
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

        for (const auto &cap : _caps) {
            auto qt_r = cap.resolution.second;
            auto qt_f = cap.fps.second;
            auto qt_c = cap.codec.second;

            auto res_match = current_res.isEmpty() || qt_r == current_res;
            auto fps_match = current_fps.isEmpty() || qt_f == current_fps;
            auto codec_match = current_codec.isEmpty() || qt_c == current_codec;

            if (current_res.isEmpty() && current_fps.isEmpty()) {
                if (codec_match) {
                    highlight_items(resolution, qt_r, true);
                    highlight_items(fps, qt_f, true);
                }
            } else {
                highlight_items(resolution, qt_r, fps_match && codec_match);
                highlight_items(fps, qt_f, res_match && codec_match);
                highlight_items(codec, qt_c, res_match && fps_match);
            }
        }
    });
    connect(name, &QCheckBox::clicked, [this, camera, resolution, fps, codec](bool checked) {
        // TODO: UGLY HACK
        auto main_window = qobject_cast<XDAQCameraControl *>(
            parentWidget()->parentWidget()->parentWidget()->parentWidget()
        );
        auto stream_mainwindow = main_window->_stream_mainwindow;

        if (checked) {
            std::string gst_cap;
            for (const auto &cap : _caps) {
                auto [gst_r, qt_r] = cap.resolution;
                auto [gst_f, qt_f] = cap.fps;
                auto [gst_c, qt_c] = cap.codec;

                if (qt_r == resolution->currentText() && qt_f == fps->currentText() &&
                    qt_c == codec->currentText()) {
                    if (gst_c == VIDEO_RAW) {
                        gst_cap = fmt::format(
                            "{},format={},width={},height={},framerate={}",
                            gst_c,
                            cap.format,
                            gst_r.width,
                            gst_r.height,
                            gst_f
                        );
                    } else if (gst_c == VIDEO_MJPEG) {
                        gst_cap = fmt::format(
                            "{},width={},height={},framerate={}",
                            gst_c,
                            gst_r.width,
                            gst_r.height,
                            gst_f
                        );
                    }
                }
            }
            camera->set_current_cap(gst_cap);

            if (!_stream_window) {
                _stream_window = new StreamWindow(camera, stream_mainwindow);
                stream_mainwindow->addDockWidget(Qt::TopDockWidgetArea, _stream_window);

                auto windows = stream_mainwindow->findChildren<StreamWindow *>();
                auto count = (int) windows.size();

                if (count == 3) {
                    stream_mainwindow->splitDockWidget(windows[0], windows[2], Qt::Vertical);
                } else if (count == 4) {
                    stream_mainwindow->splitDockWidget(windows[1], windows[3], Qt::Vertical);
                }

                stream_mainwindow->show();
                _stream_window->play();
            }
        } else {
            _stream_window->close();
            delete _stream_window;
            _stream_window = nullptr;

            if (stream_mainwindow->findChildren<StreamWindow *>().isEmpty()) {
                stream_mainwindow->close();
            } else {
                stream_mainwindow->adjustSize();
            }
        }
    });
    connect(view, &QRadioButton::toggled, [this](bool checked) {
        if (_stream_window) {
            if (checked) {
                _stream_window->show();
            } else {
                _stream_window->hide();
            }
        }
    });
}