#pragma once

#include "stream_mainwindow.h"

#include "stream_window.h"


StreamMainWindow::StreamMainWindow(QWidget *parent) : QMainWindow(parent)
{
    setDockOptions(QMainWindow::AnimatedDocks);
    // connect(this, &StreamMainWindow::dockwidget_added, [this](QDockWidget *dockwidget) {
    //     window_ids.emplace_back(dockwidget->winId());
    // });
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
}

// void StreamMainWindow::add_dockwidget(Qt::DockWidgetArea area, QDockWidget *dockwidget)
// {
//     addDockWidget(area, dockwidget);
//     emit dockwidget_added(dockwidget);
// }

void StreamMainWindow::closeEvent(QCloseEvent *e)
{
    QList<StreamWindow *> stream_windows = findChildren<StreamWindow *>();

    for (StreamWindow *window : stream_windows) {
        removeDockWidget(window);
        delete window;
        window = nullptr;
    }
    e->accept();
}
