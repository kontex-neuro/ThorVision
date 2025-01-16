#include "stream_mainwindow.h"

#include "stream_window.h"


StreamMainWindow::StreamMainWindow(QWidget *parent) : QMainWindow(parent)
{
    setDockOptions(QMainWindow::AnimatedDocks);
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    setWindowTitle(" ");
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void StreamMainWindow::closeEvent(QCloseEvent *e)
{
    auto windows = findChildren<StreamWindow *>();

    for (auto window : windows) {
        removeDockWidget(window);
        delete window;
        window = nullptr;
    }
    e->accept();
}
