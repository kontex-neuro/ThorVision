#include "mainwindow.h"

#include <QtCore/qlogging.h>
#include <QtCore/qobjectdefs.h>
#include <QtCore/qstring.h>
#include <fmt/core.h>

#include <nlohmann/json.hpp>

#include "../libxvc.h"
#include "streamwidget.h"

using nlohmann::json;

MainWindow::MainWindow() : QMainWindow(nullptr)
{
    qInfo("MainWindow::MainWindow");
    QString title = tr("KonteX Video Capture 0.0.1");
    QHBoxLayout *util_layout = new QHBoxLayout;
    QWidget *central = new QWidget(this);

    QPushButton *open_video_stream_button = new QPushButton(tr("Open video stream"));
    QPushButton *play_all_button = new QPushButton(tr("Play all"));
    QPushButton *stop_all_button = new QPushButton(tr("Stop all"));
    QPushButton *record_all_button = new QPushButton(tr("Record all"));

    setWindowTitle(title);
    central->setLayout(util_layout);
    setCentralWidget(central);

    open_video_stream_button->setFixedWidth(open_video_stream_button->sizeHint().width() + 10);
    play_all_button->setFixedWidth(play_all_button->sizeHint().width() + 10);
    stop_all_button->setFixedWidth(stop_all_button->sizeHint().width() + 10);
    record_all_button->setFixedWidth(record_all_button->sizeHint().width() + 10);

    util_layout->addWidget(open_video_stream_button);
    util_layout->addWidget(play_all_button);
    util_layout->addWidget(stop_all_button);
    util_layout->addWidget(record_all_button);

    connect(open_video_stream_button, &QPushButton::clicked, this, &MainWindow::open_video_stream);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    qInfo("MainWindow::closeEvent");

    std::string cameras = xvc::list_cameras("192.168.50.241:8000/cameras");
    if (!cameras.empty()) {
        json cameras_json = json::parse(cameras);
        for (const auto &camera : cameras_json) {
            if (camera["status"] != "idle") {
                xvc::change_camera_status(camera["id"], "idle");
            }
        }
    }

    event->accept();
}

void MainWindow::open_video_stream()
{
    qInfo("MainWindow::open_video_stream");
    auto stream_widget = new StreamWidget(this);
    addDockWidget(Qt::TopDockWidgetArea, stream_widget);
}