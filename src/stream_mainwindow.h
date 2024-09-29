#pragma once

#include <QWidget>
#include <QMainWindow>
#include <QCloseEvent>
#include <vector>

class StreamMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    StreamMainWindow(QWidget *parent = nullptr);
    ~StreamMainWindow() = default;
    // void add_dockwidget(Qt::DockWidgetArea area, QDockWidget *dockwidget);
// signals:
//     void dockwidget_added(QDockWidget *dockwidget);
protected:
    void closeEvent(QCloseEvent *event) override;
// private:
//     std::vector<WId> window_ids;
};