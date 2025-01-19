#pragma once

#include <QComboBox>
#include <QCheckBox>
#include <QWidget>
#include <string>
#include <vector>

#include "stream_window.h"
#include "xdaqvc/camera.h"


class CameraItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraItemWidget(Camera *camera, QWidget *parent = nullptr);

    QString cap() const;

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

    QCheckBox *_name;
    QComboBox *_resolution;
    QComboBox *_fps;
    QComboBox *_codec;

    std::vector<CapText> _caps;
    StreamWindow *_stream_window;
};