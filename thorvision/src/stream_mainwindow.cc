#include "stream_mainwindow.h"

#include "stream_window.h"


StreamMainWindow::StreamMainWindow(QWidget *parent) : QMainWindow(parent)
{
    setDockOptions(QMainWindow::AnimatedDocks);

    // disable fullscreen
    setMaximumSize(800, 600);

    setWindowTitle(tr(" "));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void StreamMainWindow::closeEvent(QCloseEvent *e)
{
    for (auto window : findChildren<StreamWindow *>()) {
        window->close();
        window = nullptr;
    }
    e->accept();
}