#pragma once

#include <QWidget>
#include <QMainWindow>
#include <QObject>
#include <QCloseEvent>

class StreamMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    StreamMainWindow(QWidget *parent = nullptr);
    ~StreamMainWindow() = default;

protected:
    void closeEvent(QCloseEvent *event) override;
};