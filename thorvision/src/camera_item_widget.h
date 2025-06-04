#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QString>
#include <QWidget>
#include <string>
#include <vector>

#include "name_label.h"
#include "xdaqvc/camera.h"

class CameraItemWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraItemWidget(Camera *camera, QWidget *parent = nullptr);
    ~CameraItemWidget() = default;

    QString cap() const;
    bool view() const;

    QCheckBox *_stream;
    NameLabel *_name;

signals:
    void stream_toggle(Camera *camera, bool checked);
    void view_toggle(Camera *camera, bool checked);

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

    QComboBox *_resolution;
    QComboBox *_fps;
    QComboBox *_codec;
    QRadioButton *_view;

    std::vector<CapText> _caps;
};