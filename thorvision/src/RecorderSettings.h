#pragma once

#include <spdlog/spdlog.h>

#include <QDateTime>
#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <filesystem>

#include "xdaqvc/xvc.h"

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
        _split_on = false;
        _split_length = 1;
        _split_unit_index = 0;  // 0: Seconds, 1: Minutes, 2: Hours, 3: Days

        _dir_date = true;
        _dir_name = "Experiment Name";

        const auto &documents_path =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        const auto &save_path = std::filesystem::path(documents_path.toStdString()) / "ThorVision";
        const auto &save_path_str = save_path.generic_string();

        std::error_code ec;
        if (!std::filesystem::exists(save_path, ec) &&
            !std::filesystem::create_directories(save_path, ec)) {
            spdlog::critical("Failed to create directory {}: {}", save_path_str, ec.message());
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

    [[nodiscard]] xvc::RecordConfig config() const
    {
        std::chrono::seconds duration = std::chrono::seconds(10);
        if (_split_on) {
            switch (_split_unit_index) {
            case 0: duration = std::chrono::seconds(_split_length); break;
            case 1: duration = std::chrono::minutes(_split_length); break;
            case 2: duration = std::chrono::hours(_split_length); break;
            case 3: duration = std::chrono::days(_split_length); break;
            default: spdlog::warn("Invalid split unit index"); break;
            }
        }

        const auto &parent_dir = std::filesystem::path(_save_paths.at(0).toStdString());
        const auto &dir_name =
            _dir_date
                ? std::filesystem::path(
                      QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss").toStdString()
                  )
                : std::filesystem::path(_dir_name.toStdString());
        const auto &record_dir = parent_dir / dir_name;
        // const auto &filepath = record_dir / _camera->name();

        // TODO: record_dir will be append camera name as the final filepath in start_recording(),
        // need to refactor this
        xvc::RecordConfig config(record_dir, _split_on, duration);
        return config;
    }

    bool split_on() const noexcept { return _split_on; }
    int split_length() const noexcept { return _split_length; }
    int split_unit_index() const noexcept { return _split_unit_index; }

    const QStringList &save_paths() const noexcept { return _save_paths; }
    bool dir_date() const noexcept { return _dir_date; }
    const QString &dir_name() const noexcept
    {
        // auto dir_name =
        //     _dir_date ? QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss") : _dir_name;
        return _dir_name;
    }

    void set_split_on(bool value)
    {
        if (_split_on == value) return;
        _split_on = value;
        emit settings_changed();
        log_settings();
    }
    void set_split_length(int value)
    {
        if (_split_length == value) return;
        _split_length = value;
        emit settings_changed();
        log_settings();
    }
    void set_split_unit_index(int value)
    {
        if (_split_unit_index == value) return;
        _split_unit_index = value;
        emit settings_changed();
        log_settings();
    }
    void set_save_paths(const QStringList &value)
    {
        if (_save_paths == value) return;
        _save_paths = value;
        emit settings_changed();
        log_settings();
    }
    void set_dir_date(bool value)
    {
        if (_dir_date == value) return;
        _dir_date = value;
        emit settings_changed();
        log_settings();
    }
    void set_dir_name(const QString &value)
    {
        if (_dir_name == value) return;
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

        spdlog::info(
            "Split: {} ({} {})", _split_on, _split_length, time_unit_str(_split_unit_index)
        );
        spdlog::info("Save paths: {}", _save_paths.join(", ").toStdString());
        spdlog::info("Dir Type: {}", _dir_date ? "Date" : "Custom");
        spdlog::info("Dir Name: {}", _dir_name.toStdString());
    }
};