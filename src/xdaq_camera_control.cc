#include "xdaq_camera_control.h"

#include <fmt/core.h>
#include <gst/gstelement.h>
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
#include <QString>
#include <QWidget>
#include <nlohmann/json.hpp>

#include "../libxvc.h"
#include "camera_item_widget.h"
#include "record_settings.h"
#include "stream_main_window.h"
#include <QSettings>


using nlohmann::json;


XDAQCameraControl::XDAQCameraControl() : QMainWindow(nullptr), record_settings(nullptr)
{
    stream_main_window = new StreamMainWindow(this);

    resize(600, 300);

    QWidget *central = new QWidget(this);
    QLabel *title = new QLabel(tr("XDAQ Camera Control"));
    QFont title_font("Arial", 15, QFont::Bold);
    title->setFont(title_font);

    QGridLayout *main_layout = new QGridLayout;
    QHBoxLayout *record_layout = new QHBoxLayout;
    record_button = new QPushButton(tr("REC"));

    QLabel *play_time = new QLabel(tr("00:00:00"));
    QFont play_time_font("Arial", 10);

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
    record_layout->addWidget(play_time);

    main_layout->addWidget(record_widget, 1, 0, Qt::AlignLeft);
    main_layout->addWidget(title, 0, 0, Qt::AlignLeft);
    main_layout->addWidget(record_settings_button, 1, 2, Qt::AlignRight);
    main_layout->addWidget(cameras_list, 2, 0, 2, 3);

    connect(record_button, &QPushButton::clicked, this, &XDAQCameraControl::record);
    connect(
        record_settings_button,
        &QPushButton::clicked,
        this,
        &XDAQCameraControl::open_record_settings
    );
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
            if (camera_json["status"] == 0) {  // idle
                // camera->change_status(Camera::Status::Playing);
                QListWidgetItem *item = new QListWidgetItem(cameras_list);
                CameraItemWidget *camera_item_widget = new CameraItemWidget(camera, this);
                item->setSizeHint(camera_item_widget->sizeHint());

                // for (const auto &cap : camera_json["capabilities"]) {
                //     camera_item_widget->add_capability(cap);
                // }
                cameras_list->setItemWidget(item, camera_item_widget);
            }
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

void XDAQCameraControl::record()
{
    // QPalette pal = record_button->palette();
    // pal.setColor(QPalette::Button, QColor(Qt::darkRed));
    // record_button->setAutoFillBackground(true);
    // record_button->setPalette(pal);
    // record_button->update();
    
    QList<StreamWindow *> stream_windows = findChildren<StreamWindow *>();
    QSettings settings("KonteX", "XDAQ");

    // for (StreamWindow *window : stream_windows) {
        
    //     xvc::start_recording(window->pipeline, std::string filename);
    // }

}

void XDAQCameraControl::open_record_settings()
{
    record_settings = new RecordSettings(cameras, nullptr);
    record_settings->show();

    // if (!record_settings) {

    //     record_settings->raise();
    //     record_settings->activateWindow();
    // } else {
    //     record_settings->show();
    //     record_settings->raise();
    //     record_settings->activateWindow();
    // }
}