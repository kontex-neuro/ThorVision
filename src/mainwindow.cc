#include "mainwindow.h"

#include <fmt/core.h>
#include <qnamespace.h>

#include <QFileDialog>
#include <QHBoxLayout>
#include <QList>
#include <QString>
#include <nlohmann/json.hpp>

#include "../libxvc.h"
#include "camera.h"
#include "streamwidget.h"


using nlohmann::json;

MainWindow::MainWindow() : QMainWindow(nullptr)
{
    qInfo("MainWindow::MainWindow");
    QString title = tr("KonteX Video Capture 0.0.1");
    QHBoxLayout *util_layout = new QHBoxLayout;
    QWidget *central = new QWidget(this);

    QPushButton *open_video_stream_button = new QPushButton(tr("Open Video Stream"));
    // open_video_stream_button->setStyleSheet(
    //     "QPushButton {"
    //     "  border-radius: 15px;"        // Make the button's corners rounded
    //     "  background-color: #4CAF50;"  // Green background color (you can customize it)
    //     "  color: white;"               // Text color
    //     "  padding: 10px 20px;"         // Padding inside the button
    //     "  font-size: 16px;"            // Font size
    //     "  border: 1px solid #388E3C;"  // Border color
    //     "}"
    //     "QPushButton:hover {"
    //     "  background-color: #45A049;"  // Hover effect
    //     "}"
    //     "QPushButton:pressed {"
    //     "  background-color: #388E3C;"  // Pressed button effect
    //     "}"
    // );
    QPushButton *play_all_button = new QPushButton(tr("Play All"));
    QPushButton *stop_all_button = new QPushButton(tr("Stop All"));
    QPushButton *record_all_button = new QPushButton(tr("Record All"));
    QPushButton *open_all_button = new QPushButton(tr("Open all"));

    setWindowTitle(title);
    central->setLayout(util_layout);
    setCentralWidget(central);

    open_video_stream_button->setFixedWidth(open_video_stream_button->sizeHint().width() + 10);
    play_all_button->setFixedWidth(play_all_button->sizeHint().width() + 10);
    stop_all_button->setFixedWidth(stop_all_button->sizeHint().width() + 10);
    record_all_button->setFixedWidth(record_all_button->sizeHint().width() + 10);
    open_all_button->setFixedWidth(open_all_button->sizeHint().width() + 10);

    util_layout->addWidget(open_video_stream_button);
    util_layout->addWidget(play_all_button);
    util_layout->addWidget(stop_all_button);
    util_layout->addWidget(record_all_button);
    util_layout->addWidget(open_all_button);

    connect(open_video_stream_button, &QPushButton::clicked, this, &MainWindow::open_video_stream);
    connect(play_all_button, &QPushButton::clicked, this, &MainWindow::play_all);
    connect(stop_all_button, &QPushButton::clicked, this, &MainWindow::stop_all);
    connect(record_all_button, &QPushButton::clicked, this, &MainWindow::record_all);
    connect(open_all_button, &QPushButton::clicked, this, &MainWindow::open_all);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    qInfo("MainWindow::closeEvent");

    // std::string cameras = xvc::list_cameras("192.168.177.100:8000/cameras");
    // if (!cameras.empty()) {
    //     json cameras_json = json::parse(cameras);
    //     for (const auto &camera : cameras_json) {
    //         if (camera["status"] != "idle") {
    //             xvc::change_camera_status(camera["id"], "idle");
    //         }
    //     }
    // }
    for (auto stream_widget : stream_widgets) {
        // if () {
        //     xvc::change_camera_status(camera["id"], );
        // }
        stream_widget->camera.change_status(Camera::Status::Idle);
        stream_widget->camera.stop();
    }

    e->accept();
}

void MainWindow::open_video_stream()
{
    qInfo("MainWindow::open_video_stream");
    auto stream_widget = new StreamWidget(this);
    stream_widgets.push_back(stream_widget);
    addDockWidget(Qt::TopDockWidgetArea, stream_widget);
}

void MainWindow::play_all()
{
    qInfo("MainWindow::play_all");
    // if (stream_widgets.empty()) {
    // std::string cameras = xvc::list_cameras("192.168.177.100:8000/idle");
    std::string cameras = Camera::list_cameras("192.168.177.100:8000/idle");

    if (!cameras.empty()) {
        json cameras_json = json::parse(cameras);
        for (int i = 0; i < cameras_json.size(); ++i) {
            open_video_stream();
        }
    }
    // }
    for (auto stream_widget : stream_widgets) {
        if (stream_widget->camera.get_status() == Camera::Status::Occupied) {
            qInfo("play all");
            stream_widget->start_stream();
            stream_widget->camera.change_status(Camera::Status::Playing);
        }
    }
}

void MainWindow::stop_all()
{
    qInfo("MainWindow::stop_all");
    for (auto stream_widget : stream_widgets) {
        if (stream_widget->camera.get_status() == Camera::Status::Playing) {
            qInfo("stop all");
            stream_widget->stop_stream();
            stream_widget->camera.change_status(Camera::Status::Playing);
        }
    }
}

void MainWindow::record_all() { qInfo("MainWindow::record_all"); }

void MainWindow::open_all()
{
    qInfo("MainWindow::open_all");
    QStringList filepaths =
        QFileDialog::getOpenFileNames(this, tr("Select Video Files"), "", "Video Files (*.mkv)");
    for (const QString &filepath : filepaths) {
        auto stream_widget = new StreamWidget(nullptr);
        stream_widgets.push_back(stream_widget);
        stream_widget->open_video(filepath);
        addDockWidget(Qt::TopDockWidgetArea, stream_widget);

        // stream_widget->set_use_camera(false);
        fmt::println("{}", filepath.toStdString());
    }
}