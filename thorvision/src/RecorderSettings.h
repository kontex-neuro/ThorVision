#pragma once

#ifndef RECORDER_SETTINGS_H
#define RECORDER_SETTINGS_H

#include <spdlog/spdlog.h>

#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <filesystem>

class RecorderSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool split_on READ split_on WRITE set_split_on NOTIFY settings_changed)
    Q_PROPERTY(int split_length READ split_length WRITE set_split_length NOTIFY settings_changed)
    Q_PROPERTY(
        int split_unit_index READ split_unit_index WRITE set_split_unit_index NOTIFY
            settings_changed
    )
    Q_PROPERTY(QStringList save_paths READ save_paths WRITE set_save_paths NOTIFY settings_changed)
    Q_PROPERTY(bool dir_date READ dir_date WRITE set_dir_date NOTIFY settings_changed)
    Q_PROPERTY(QString dir_name READ dir_name WRITE set_dir_name NOTIFY settings_changed)

public:
    explicit RecorderSettings(QObject *parent = nullptr) : QObject(parent)
    {
        namespace fs = std::filesystem;

        _split_on = false;
        _split_length = 1;
        _split_unit_index = 0;  // 0: Seconds, 1: Minutes, 2: Hours, 3: Days

        _dir_date = true;
        _dir_name = "Experiment Name";

        const auto documents_path =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        const auto save_path = fs::path(documents_path.toStdString()) / "ThorVision";
        const auto save_path_str = save_path.generic_string();

        std::error_code ec;
        if (!fs::exists(save_path, ec)) {
            spdlog::info("Creating save directory: {}", save_path_str);
            if (!fs::create_directories(save_path, ec)) {
                spdlog::error("Failed to create directory {}: {}", save_path_str, ec.message());
            }
        } else if (ec) {
            spdlog::warn("Error checking directory {}: {}", save_path_str, ec.message());
        }
        _save_paths = QStringList(QString::fromStdString(save_path_str));
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

        spdlog::debug(
            "Split: {} ({} {})", _split_on, _split_length, time_unit_str(_split_unit_index)
        );
        spdlog::debug("Save paths: {}", _save_paths.join(", ").toStdString());
        spdlog::debug("Dir Type: {}", _dir_date ? "Date" : "Custom");
        spdlog::debug("Dir Name: {}", _dir_name.toStdString());
    }
};

#endif