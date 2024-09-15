#pragma once

#include <QtCore/qobjectdefs.h>

#include <QMainWindow>
#include <QtWidgets>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow() = default;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void open_video_stream();
};