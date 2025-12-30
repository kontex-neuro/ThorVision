#pragma once

#ifndef CONFIG_H
#define CONFIG_H

#include <spdlog/spdlog.h>

#include <QDir>
#include <QFile>
#include <QObject>
#include <QSaveFile>
#include <QSettings>
#include <QUrl>
#include <nlohmann/json.hpp>

#include "CameraModel.h"
#include "RecorderSettings.h"

using json = nlohmann::json;

class Config : public QObject
{
    Q_OBJECT
public:
    explicit Config(
        RecorderSettings *recorder_settings, CameraModel *camera_model, QObject *parent = nullptr
    )
        : QObject(parent), _recorder_settings(recorder_settings), _camera_model(camera_model)
    {
    }

    Q_INVOKABLE bool export_to_file(const QUrl &file_url)
    {
        const auto &path = file_url.toLocalFile();
        spdlog::info("Exported profile to {}", path.toStdString());
        if (path.isEmpty()) {
            spdlog::error("Invalid file URL");
            return false;
        }

        read();

        const auto &json_data = QByteArray::fromStdString(_root.dump(2));

        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            spdlog::error("Failed to create directory for path: {}", path.toStdString());
            return false;
        }

        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            spdlog::error("Failed to open file for writing: {}", path.toStdString());
            return false;
        }
        file.write(json_data);
        if (!file.commit()) {
            spdlog::error("Failed to commit file");
            return false;
        }

        return true;
    }

    Q_INVOKABLE bool import_from_file(const QUrl &file_url)
    {
        spdlog::info("Importing settings from file: {}", file_url.toString().toStdString());

        const auto &path = file_url.toLocalFile();
        if (path.isEmpty()) {
            spdlog::error("Invalid file URL");
            emit import_failed();
            return false;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            spdlog::error("Failed to open file for reading: {}", path.toStdString());
            emit import_failed();
            return false;
        }

        const auto &json_data = file.readAll();
        file.close();

        try {
            const auto &root = json::parse(json_data);

            auto success = set(root);
            if (!success) {
                emit import_failed();
            }

            return success;
        } catch (const json::exception &e) {
            spdlog::error("JSON error: {}", e.what());
            emit import_failed();
            return false;
        }

        return true;
    }

    bool set(const json &root)
    {
        spdlog::info("Setting profile from JSON");
        const auto &camera_json = root["camera"];
        std::unordered_map<std::string, int> saved_counts;
        std::unordered_map<std::string, int> current_counts;
        std::unordered_map<std::string, std::deque<int>> id_indices;

        for (const auto &cam : camera_json) {
            ++saved_counts[cam["device_id"].get<std::string>()];
        }

        for (auto i = 0; i < _camera_model->rowCount(); ++i) {
            const auto &id = _camera_model->get(i)["device_id"].toString().toStdString();
            ++current_counts[id];
            id_indices[id].push_back(i);
        }

        if (saved_counts != current_counts) {
            spdlog::warn("Camera configuration mismatch!");

            spdlog::warn("Saved config:");
            for (const auto &[id, count] : saved_counts) {
                spdlog::warn("  {} x{}", id, count);
            }
            spdlog::warn("Currently connected:");
            for (const auto &[id, count] : current_counts) {
                spdlog::warn("  {} x{}", id, count);
            }

            emit import_failed();
            return false;
        }

        const auto &rec = root["record"];
        _recorder_settings->set_split_on(rec["split_on"]);
        _recorder_settings->set_split_length(rec["split_length"]);
        _recorder_settings->set_split_unit_index(rec["split_unit_index"]);

        QStringList paths;
        for (const auto &p : rec["save_paths"])
            paths << QString::fromStdString(p.get<std::string>());
        _recorder_settings->set_save_paths(paths);

        _recorder_settings->set_dir_date(rec["dir_date"]);
        _recorder_settings->set_dir_name(
            QString::fromStdString(rec["dir_name"].get<std::string>())
        );

        for (const auto &cam_json : camera_json) {
            const auto id = cam_json["device_id"].get<std::string>();
            auto index = id_indices[id].front();
            id_indices[id].pop_front();

            const auto cam_data = _camera_model->get(index);
            auto camera_item = cam_data["camera_item"].value<CameraItem *>();

            camera_item->set_name(QString::fromStdString(cam_json["name"].get<std::string>()));
            camera_item->set_cap(QString::fromStdString(cam_json["cap"].get<std::string>()));
            camera_item->set_codec(QString::fromStdString(cam_json["codec"].get<std::string>()));
        }

        return true;
    }

    void read()
    {
        json record;
        record["split_on"] = _recorder_settings->split_on();
        record["split_length"] = _recorder_settings->split_length();
        record["split_unit_index"] = _recorder_settings->split_unit_index();
        json save_paths;
        for (const auto &path : _recorder_settings->save_paths()) {
            save_paths.push_back(path.toStdString());
        }
        record["save_paths"] = save_paths;
        record["dir_date"] = _recorder_settings->dir_date();
        record["dir_name"] = _recorder_settings->dir_name().toStdString();

        json camera;
        for (auto i = 0; i < _camera_model->rowCount(); ++i) {
            const auto cam_data = _camera_model->get(i);
            json cam_json;
            cam_json["device_id"] = cam_data["device_id"].toString().toStdString();
            cam_json["name"] = cam_data["name"].toString().toStdString();
            cam_json["cap"] = cam_data["cap"].toString().toStdString();
            cam_json["codec"] = cam_data["codec"].toString().toStdString();
            camera.push_back(cam_json);
        }

        _root["record"] = record;
        _root["camera"] = camera;
    }

    Q_INVOKABLE bool set_default(const QUrl &file_url)
    {
        const auto &path = file_url.toLocalFile();
        if (path.isEmpty()) {
            spdlog::warn("Default profile path is empty");
            return false;
        }

        QSettings settings;
        settings.setValue("default_config_path", path);

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            spdlog::warn("Cannot open default config");
            return false;
        }

        try {
            const auto root = json::parse(file.readAll());
            bool success = set(root);
            // if (!success) emit default_import_failed();
            return success;
        } catch (...) {
            // emit default_import_failed();
            return false;
        }
    }

    bool load_default()
    {
        QSettings settings;
        const auto &path = settings.value("default_config_path", "").toString();
        spdlog::info("Loading default profile {}", path.toStdString());

        if (path.isEmpty()) return false;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            // emit default_import_failed();
            spdlog::error("Cannot open default profile");
            return false;
        }

        try {
            const auto root = json::parse(file.readAll());
            if (!set(root)) {
                spdlog::error("Cannot set default profile");
                // emit default_import_failed();
            }
            return true;
        } catch (...) {
            spdlog::error("Error parsing default profile");
            // emit default_import_failed();
        }
        return false;
    }

signals:
    void import_failed();

private:
    json _root;

    RecorderSettings *_recorder_settings;
    CameraModel *_camera_model;
};

#endif