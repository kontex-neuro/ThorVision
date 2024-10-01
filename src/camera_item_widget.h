#pragma once

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <string>

#include <QCheckBox>
#include <QComboBox>
#include <QWidget>
#include "camera.h"
#include "stream_window.h"

struct Capability {
    std::string codec;
    std::string format;
    // std::string width;
    // std::string height;
    // std::string framerate;
    std::string resolution;
    std::string fps;
};

class CameraItemWidget : public QWidget
{
    Q_OBJECT
public:
    CameraItemWidget(Camera* _camera, QWidget *parent = nullptr);

    void parse(const std::string &capability);
    std::string dump();
    void load_caps();

private:
    std::vector<Capability> capabilities;
    StreamWindow *stream_window;
    Camera* camera;

    std::unordered_map<QString, std::string> display_gst;
    std::unordered_map<QString, std::pair<std::string, std::string>> display_gst_resolution;

    // std::pair<QString, std::string> codec_pair;
    // std::pair<QString, std::string> resolution_pair;
    // std::pair<QString, std::string> fps_pair;

    std::unordered_set<std::string> codec_set;
    std::unordered_set<std::string> resolution_set;
    std::unordered_set<std::string> fps_set;
    std::unordered_set<std::string> video_format;

    QCheckBox *name;
    QComboBox *resolution;
    QComboBox *fps;
    QComboBox *codec;
    QCheckBox *audio;

private slots:
    void play();
    // void pause();
};