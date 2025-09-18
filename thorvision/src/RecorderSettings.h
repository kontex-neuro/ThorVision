#pragma once

#ifndef RECORDER_SETTINGS_H
#define RECORDER_SETTINGS_H

#include <spdlog/spdlog.h>

#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

class RecorderSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool split_on READ split_on WRITE set_split_on NOTIFY settings_changed)
    Q_PROPERTY(int split_length READ split_length WRITE set_split_length NOTIFY settings_changed)
    Q_PROPERTY(int split_unit_index READ split_unit_index WRITE set_split_unit_index NOTIFY
                   settings_changed)
    Q_PROPERTY(bool loop_on READ loop_on WRITE set_loop_on NOTIFY settings_changed)
    Q_PROPERTY(int max_files READ max_files WRITE set_max_files NOTIFY settings_changed)
    Q_PROPERTY(QStringList save_paths READ save_paths WRITE set_save_paths NOTIFY settings_changed)
    Q_PROPERTY(bool dir_date READ dir_date WRITE set_dir_date NOTIFY settings_changed)
    Q_PROPERTY(QString dir_name READ dir_name WRITE set_dir_name NOTIFY settings_changed)

public:
    explicit RecorderSettings(QObject *parent = nullptr) : QObject(parent)
    {
        _split_on = false;
        _split_length = 1;
        _split_unit_index = 0;  // 0: Seconds, 1: Minutes, 2: Hours, 3: Days
        _loop_on = false;
        _max_files = 1;
        _save_paths =
            QStringList(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        _dir_date = false;
        _dir_name = "directory_name";
    }
    ~RecorderSettings() = default;

    Q_INVOKABLE void update_save_path_history(const QString &path)
    {
        if (path.trimmed().isEmpty()) return;

        _save_paths.removeAll(path);
        _save_paths.prepend(path);
        while (_save_paths.size() > 10) {
            _save_paths.removeLast();
        }

        emit settings_changed();
        log_settings();
    }

    bool split_on() const { return _split_on; }
    int split_length() const { return _split_length; }
    int split_unit_index() const { return _split_unit_index; }
    bool loop_on() const { return _loop_on; }
    int max_files() const { return _max_files; }
    QStringList save_paths() const { return _save_paths; }
    bool dir_date() const { return _dir_date; };
    QString dir_name() const { return _dir_name; }

    void set_split_on(bool value)
    {
        _split_on = value;
        emit settings_changed();
        log_settings();
    }
    void set_split_length(int value)
    {
        _split_length = value;
        emit settings_changed();
        log_settings();
    }
    void set_split_unit_index(int value)
    {
        _split_unit_index = value;
        emit settings_changed();
        log_settings();
    }
    void set_loop_on(bool value)
    {
        _loop_on = value;
        emit settings_changed();
        log_settings();
    }
    void set_max_files(int value)
    {
        _max_files = value;
        emit settings_changed();
        log_settings();
    }
    void set_save_paths(const QStringList &value)
    {
        _save_paths = value;
        emit settings_changed();
        log_settings();
    }
    void set_dir_date(bool value)
    {
        _dir_date = value;
        emit settings_changed();
        log_settings();
    }
    void set_dir_name(const QString &value)
    {
        _dir_name = value;
        emit settings_changed();
        log_settings();
    }

signals:
    void settings_changed();

private:
    bool _split_on;
    int _split_length;
    int _split_unit_index;

    bool _loop_on;
    int _max_files;

    QStringList _save_paths;
    bool _dir_date;
    QString _dir_name;

    void log_settings() const
    {
        auto time_unit_str = [](int index) {
            switch (index) {
            case 0: return "Seconds";
            case 1: return "Minutes";
            case 2: return "Hours";
            case 3: return "Days";
            default: return "Minutes";
            }
        };

        spdlog::info(
            "Split: {} ({} {})", _split_on, _split_length, time_unit_str(_split_unit_index)
        );
        spdlog::info("Loop: {} (max files: {})", _loop_on, _max_files);
        spdlog::info("Save paths: {}", _save_paths.join(", ").toStdString());
        spdlog::info("Dir Date: {}", _dir_date ? "Date" : "Custom");
        spdlog::info("Dir Name: {}", _dir_name.toStdString());
    }
};

#endif