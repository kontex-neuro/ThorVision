#pragma once

#include <QByteArray>
#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <QUrl>
#include <filesystem>
#include <optional>
#include <string_view>

#include "CameraModel.h"
#include "RecorderSettings.h"

class Config : public QObject
{
    Q_OBJECT

public:
    explicit Config(
        RecorderSettings &recorder_settings, CameraModel &camera_model, QObject *parent = nullptr
    )
        : QObject(parent), _recorder_settings(&recorder_settings), _camera_model(&camera_model)
    {
        const auto &documents_path =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        
        _save_path = std::filesystem::path(documents_path.toStdString()) / "ThorVision" / "configs";

        std::error_code ec;
        if (!std::filesystem::exists(_save_path, ec) &&
            !std::filesystem::create_directories(_save_path, ec)) {
            spdlog::critical(
                "Failed to create directory {}: {}", _save_path.generic_string(), ec.message()
            );
        }
    }

    Q_INVOKABLE const QString &license_text() const;
    Q_INVOKABLE bool export_to_file(const QUrl &file_url) const;
    Q_INVOKABLE bool import_from_file(const QUrl &file_url);
    Q_INVOKABLE bool set_default(const QUrl &file_url);
    Q_INVOKABLE QUrl default_config_path() const noexcept
    {
        return QUrl::fromLocalFile(QString::fromStdString(_save_path.string()));
    }

    bool has_default_config() const;
    bool load_default();

signals:
    void import_failed(const QString &reason);

private:
    QPointer<RecorderSettings> _recorder_settings;
    QPointer<CameraModel> _camera_model;

    std::filesystem::path _save_path;

    std::optional<QByteArray> read_file(const QString &path) const;
    bool set(std::string_view json);
    bool load_config_from_path(const QString &path);
};