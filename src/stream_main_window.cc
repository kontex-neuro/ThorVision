#pragma once

#include "stream_main_window.h"
// #include <QDockWidget>
#include "stream_window.h"

StreamMainWindow::StreamMainWindow(QWidget *parent) : QMainWindow(parent) {}

void StreamMainWindow::closeEvent(QCloseEvent *e)
{
    QList<StreamWindow *> stream_windows = findChildren<StreamWindow *>();

    for (StreamWindow *window : stream_windows) {
        removeDockWidget(window);
        delete window;
    }
    e->accept();
}
