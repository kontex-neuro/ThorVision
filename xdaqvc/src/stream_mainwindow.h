#pragma once

#include <QCloseEvent>
#include <QMainWindow>
#include <QWidget>


class StreamMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit StreamMainWindow(QWidget *parent = nullptr);
    ~StreamMainWindow() = default;

protected:
    void closeEvent(QCloseEvent *e) override;
};