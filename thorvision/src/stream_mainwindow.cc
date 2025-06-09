#include "stream_mainwindow.h"

#include "stream_window.h"

StreamMainWindow::StreamMainWindow(QWidget *parent) : QMainWindow(parent)
{
    spdlog::info("Creating StreamMainWindow");
    setDockOptions(QMainWindow::AnimatedDocks);

    // disable fullscreen
    setMaximumSize(480, 360);

    setWindowTitle(tr(" "));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void StreamMainWindow::closeEvent(QCloseEvent *e)
{
    for (auto window : findChildren<StreamWindow *>()) {
        window->close();
    }
    e->accept();
}