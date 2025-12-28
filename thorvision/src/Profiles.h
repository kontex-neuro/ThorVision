#pragma once

#ifndef PROFILES_H
#define PROFILES_H

#include <spdlog/spdlog.h>

#include <QDir>
#include <QFile>
#include <QObject>
#include <QSaveFile>
#include <QUrl>
#include <nlohmann/json.hpp>

#include "CameraModel.h"
#include "RecorderSettings.h"

using json = nlohmann::json;

class Profiles : public QObject
{
    Q_OBJECT
public:
    explicit Profiles(
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
            return false;
        }

        QFile file(path);
        if (!file.exists()) {
            spdlog::error("File does not exist: {}", path.toStdString());
            return false;
        }
        if (!file.open(QIODevice::ReadOnly)) {
            spdlog::error("Failed to open file for reading: {}", path.toStdString());
            return false;
        }

        const auto &json_data = file.readAll();
        file.close();

        try {
            const auto &root = json::parse(json_data);

            set(root);

            return true;
        } catch (const json::exception &e) {
            spdlog::error("JSON error: {}", e.what());
            return false;
        }

        return true;
    }

    void set(const json &root)
    {
        const auto &rec = root["record"];
        _recorder_settings->set_split_on(rec["split_on"]);
        _recorder_settings->set_split_length(rec["split_length"]);
        _recorder_settings->set_split_unit_index(rec["split_unit_index"]);
        QStringList paths;
        for (const auto &p : rec["save_paths"]) {
            paths << QString::fromStdString(p.get<std::string>());
        }
        _recorder_settings->set_save_paths(paths);
        _recorder_settings->set_dir_date(rec["dir_date"]);
        _recorder_settings->set_dir_name(
            QString::fromStdString(rec["dir_name"].get<std::string>())
        );

        const auto &camera_settings = root["camera"];
        for (unsigned long i = 0; i < camera_settings.size(); ++i) {
            const auto& cam_json = camera_settings[i];
            const auto cam_data = _camera_model->get(i);
            auto camera_item = cam_data["camera_item"].value<CameraItem *>();
            camera_item->set_name(QString::fromStdString(cam_json["name"].get<std::string>()));
            camera_item->set_cap(QString::fromStdString(cam_json["cap"].get<std::string>()));
            camera_item->set_codec(QString::fromStdString(cam_json["codec"].get<std::string>()));
        }
    }
    void read()
    {
        json record_settings;
        record_settings["split_on"] = _recorder_settings->split_on();
        record_settings["split_length"] = _recorder_settings->split_length();
        record_settings["split_unit_index"] = _recorder_settings->split_unit_index();
        json save_paths;
        for (const auto &path : _recorder_settings->save_paths()) {
            save_paths.push_back(path.toStdString());
        }
        record_settings["save_paths"] = save_paths;
        record_settings["dir_date"] = _recorder_settings->dir_date();
        record_settings["dir_name"] = _recorder_settings->dir_name().toStdString();

        json camera_settings;
        for (auto i = 0; i < _camera_model->rowCount(); ++i) {
            const auto cam_data = _camera_model->get(i);
            json cam_json;
            cam_json["name"] = cam_data["name"].toString().toStdString();
            cam_json["cap"] = cam_data["cap"].toString().toStdString();
            cam_json["codec"] = cam_data["codec"].toString().toStdString();
            camera_settings.push_back(cam_json);
        }

        _root["record"] = record_settings;
        _root["camera"] = camera_settings;
    }

private:
    json _root;

    RecorderSettings *_recorder_settings;
    CameraModel *_camera_model;
};

#endif