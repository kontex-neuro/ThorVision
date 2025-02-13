#pragma once

#include <QApplication>


class App final : public QApplication
{
public:
    App(int &argc, char **argv);
    ~App() = default;

    bool notify(QObject *receiver, QEvent *e) override;
};