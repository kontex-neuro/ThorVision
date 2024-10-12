#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QWidget>
#include <string>
#include <vector>

#include "../src/camera.h"
#include "stream_window.h"


class CameraItemWidget : public QWidget
{
    Q_OBJECT
public:
    CameraItemWidget(Camera *_camera, QWidget *parent = nullptr);

private:
    struct Resolution {
        int width;
        int height;

        bool operator<(const Resolution &other) const
        {
            if (width == other.width) {
                return height < other.height;
            }
            return width < other.width;
        }
    };

    struct CapText {
        std::pair<std::string, QString> codec;
        std::string format;
        std::pair<Resolution, QString> resolution;
        std::pair<std::string, QString> fps;
    };

    std::vector<CapText> caps;
    StreamWindow *stream_window;
    Camera *camera;
};