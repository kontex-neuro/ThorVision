#include "xdaq_camera_control.h"

#include <fmt/core.h>
#include <gst/gstelement.h>
#include <gst/gstpipeline.h>
#include <qnamespace.h>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

// #include <QList>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <nlohmann/json.hpp>

#include "../libxvc.h"
#include "camera_item_widget.h"
#include "record_settings.h"


using nlohmann::json;


XDAQCameraControl::XDAQCameraControl()
    : QMainWindow(nullptr), stream_mainwindow(nullptr), record_settings(nullptr)
{
    setMinimumSize(600, 300);
    resize(600, 300);
    stream_mainwindow = new StreamMainWindow(this);

    QWidget *central = new QWidget(this);
    QLabel *title = new QLabel(tr("XDAQ Camera Control"));
    QFont title_font("Arial", 15, QFont::Bold);
    title->setFont(title_font);

    QGridLayout *main_layout = new QGridLayout;
    QHBoxLayout *record_layout = new QHBoxLayout;
    record_button = new QPushButton(tr("REC"));

    QTimer *timer = new QTimer(this);
    QLabel *record_time = new QLabel(tr("00:00:00"));
    QFont record_time_font("Arial", 10);
    record_time->setFont(record_time_font);

    QPushButton *record_settings_button = new QPushButton(tr("SETTINGS"));

    cameras_list = new QListWidget(this);
    load_cameras();

    record_settings = new RecordSettings(cameras, nullptr);

    record_button->setFixedWidth(record_button->sizeHint().width());

    setWindowTitle(tr(" "));
    setCentralWidget(central);
    // setWindowFlags(Qt::CustomizeWindowHint);

    central->setLayout(main_layout);
    QWidget *record_widget = new QWidget(this);
    record_widget->setLayout(record_layout);
    record_layout->addWidget(record_button);
    record_layout->addWidget(record_time);

    main_layout->addWidget(record_widget, 1, 0, Qt::AlignLeft);
    main_layout->addWidget(title, 0, 0, Qt::AlignLeft);
    main_layout->addWidget(record_settings_button, 1, 2, Qt::AlignRight);
    main_layout->addWidget(cameras_list, 2, 0, 2, 3);

    bool recording = false;

    connect(timer, &QTimer::timeout, [this, record_time]() {
        ++elapsed_time;

        int hours = elapsed_time / 3600;
        int minutes = (elapsed_time % 3600) / 60;
        int seconds = elapsed_time % 60;

        record_time->setText(
            QString::fromStdString(fmt::format("{:02}:{:02}:{:02}", hours, minutes, seconds))
        );
    });
    connect(record_button, &QPushButton::clicked, [this, timer, recording, record_time]() mutable {
        if (!recording) {
            recording = true;
            elapsed_time = 0;
            record_time->setText(tr("00:00:00"));
            timer->start(1000);
            record_button->setText(tr("STOP"));

            auto save_path =
                QSettings("KonteX", "VC").value("save_path", QDir::currentPath()).toString();
            auto dir_name =
                QSettings("KonteX", "VC")
                    .value("dir_name", QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"))
                    .toString();
            QDir dir(save_path);
            if (!dir.exists(dir_name)) {
                if (!dir.mkdir(dir_name)) {
                    qDebug() << "Failed to create directory: " << save_path + "/" + dir_name;
                    return;
                }
            }
            QList<StreamWindow *> stream_windows =
                stream_mainwindow->findChildren<StreamWindow *>();
            for (StreamWindow *window : stream_windows) {
                fmt::println(
                    "save_path = {}, dir_name = {}", save_path.toStdString(), dir_name.toStdString()
                );
                xvc::start_recording(
                    GST_PIPELINE(window->pipeline),
                    save_path.toStdString() + "/" + dir_name.toStdString() + "/" +
                        window->camera->get_name()
                );
            }
        } else {
            recording = false;
            QList<StreamWindow *> stream_windows =
                stream_mainwindow->findChildren<StreamWindow *>();
            for (StreamWindow *window : stream_windows) {
                xvc::stop_recording(GST_PIPELINE(window->pipeline));
            }
            record_button->setText(tr("REC"));
            timer->stop();
        }
    });
    connect(record_settings_button, &QPushButton::clicked, [this]() { record_settings->show(); });
}

void XDAQCameraControl::load_cameras()
{
    std::string cameras_str = Camera::list_cameras("192.168.177.100:8000/idle");
    if (!cameras_str.empty()) {
        json cameras_json = json::parse(cameras_str);
        for (const auto &camera_json : cameras_json) {
            Camera *camera = new Camera(camera_json["id"], camera_json["name"]);
            for (const auto &cap : camera_json["capabilities"]) {
                camera->add_capability(cap);
            }
            QListWidgetItem *item = new QListWidgetItem(cameras_list);
            CameraItemWidget *camera_item_widget = new CameraItemWidget(camera, this);
            item->setSizeHint(camera_item_widget->sizeHint());
            cameras_list->setItemWidget(item, camera_item_widget);
            cameras.emplace_back(camera);
        }
    }
}

void XDAQCameraControl::mousePressEvent(QMouseEvent *e)
{
    if (record_settings && !record_settings->geometry().contains(e->pos())) {
        record_settings->close();
    }
}

void XDAQCameraControl::closeEvent(QCloseEvent *e)
{
    for (auto camera : cameras) {
        camera->stop();
    }
    e->accept();
}