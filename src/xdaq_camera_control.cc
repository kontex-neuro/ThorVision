#include "xdaq_camera_control.h"

#include <fmt/core.h>
#include <gst/gstelement.h>
#include <gst/gstpipeline.h>
#include <qcombobox.h>
#include <qnamespace.h>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

// #include <QList>
#include <QCheckBox>
#include <QComboBox>
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
#include "camera_record_widget.h"
#include "record_settings.h"


using nlohmann::json;


XDAQCameraControl::XDAQCameraControl()
    : QMainWindow(nullptr), stream_mainwindow(nullptr), record_settings(nullptr)
{
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
    int elapsed_time = 0;

    QPushButton *record_settings_button = new QPushButton(tr("SETTINGS"));

    cameras_list = new QListWidget(this);
    load_cameras();

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

    connect(timer, &QTimer::timeout, [this, elapsed_time, record_time]() mutable {
        ++elapsed_time;

        int hours = elapsed_time / 3600;
        int minutes = (elapsed_time % 3600) / 60;
        int seconds = elapsed_time % 60;

        record_time->setText(
            QString::fromStdString(fmt::format("{:02}:{:02}:{:02}", hours, minutes, seconds))
        );
    });
    connect(
        record_button,
        &QPushButton::clicked,
        [this, elapsed_time, timer](bool checked) mutable {
            // QPalette pal = record_button->palette();
            // pal.setColor(QPalette::Button, QColor(Qt::darkRed));
            // record_button->setAutoFillBackground(true);
            // record_button->setPalette(pal);
            // record_button->update();
            if (checked) {
                record_button->setText(tr("REC"));
                elapsed_time = 0;
                timer->start(1000);
                QList<StreamWindow *> stream_windows =
                    stream_mainwindow->findChildren<StreamWindow *>();
                for (StreamWindow *window : stream_windows) {
                    QSettings settings("KonteX", "XDAQ");
                    std::string file_path = settings.value("file_path").toString().toStdString();
                    xvc::start_recording(GST_PIPELINE(window->pipeline), file_path);
                }
            } else {
                record_button->setText(tr("STOP"));
                timer->stop();
            }
            // QList<StreamWindow *> record_setting = record_settings->findChildren<StreamWindow
            // *>(); HACK: one cameras_list is member of xdaq_camera_control.h another cameras_list
            // is memeber of record_settings.h for (int i = 0; i < cameras_list->count(); ++i) {
            //     QListWidgetItem *item = record_settings->cameras_list->item(i);
            //     CameraRecordWidget *camera_record_widget =
            //         qobject_cast<CameraRecordWidget *>(cameras_list->itemWidget(item));

            //     // for (StreamWindow *window : stream_windows) {
            //     //     if (camera_record_widget->clicked()) {
            //     //         QSettings settings("KonteX", "XDAQ");
            //     //         std::string file_path =
            //     settings.value("file_path").toString().toStdString();
            //     //         xvc::start_recording(GST_PIPELINE(window->pipeline), file_path);
            //     //     }
            //     // }
            //     // camera_record_widget
            //     // stream_mainwindow->findChild(camera_record_widget->get_name());
            //     if (camera_record_widget->clicked()) {
            //         QSettings settings("KonteX", "XDAQ");
            //         std::string file_path = settings.value("file_path").toString().toStdString();
            //         // xvc::start_recording(GST_PIPELINE(window->pipeline), file_path);
            //     }
            // }
        }
    );
    connect(record_settings_button, &QPushButton::clicked, [this]() {
        record_settings = new RecordSettings(cameras, nullptr);
        record_settings->show();
    });
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
            // if (camera_json["status"] == 0) {  // idle
            // camera->change_status(Camera::Status::Playing);
            QListWidgetItem *item = new QListWidgetItem(cameras_list);
            CameraItemWidget *camera_item_widget = new CameraItemWidget(camera, this);
            item->setSizeHint(camera_item_widget->sizeHint());

            // for (const auto &cap : camera_json["capabilities"]) {
            //     camera_item_widget->add_capability(cap);
            // }
            // style()->standardIcon(QStyle::SP_MediaPause);
            cameras_list->setItemWidget(item, camera_item_widget);
            // }
            cameras.emplace_back(camera);
        }
    }
}

void XDAQCameraControl::mousePressEvent(QMouseEvent *e)
{
    if (record_settings && !record_settings->geometry().contains(e->pos())) {
        record_settings->close();
        delete record_settings;
        record_settings = nullptr;
    }
}

void XDAQCameraControl::closeEvent(QCloseEvent *e)
{
    // for (auto stream_widget : stream_widgets) {
    //     stream_widget->camera.change_status(Camera::Status::Idle);
    //     stream_widget->camera.stop();
    // }

    e->accept();
}