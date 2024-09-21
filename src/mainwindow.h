#pragma once

#include <QMainWindow>
// #include <QtWidgets>
#include <QCloseEvent>
#include <vector>

#include "streamwidget.h"


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow() = default;

private:
    std::vector<StreamWidget *> stream_widgets;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void open_video_stream();
    void play_all();
    void stop_all();
    void record_all();
    void open_all();
};