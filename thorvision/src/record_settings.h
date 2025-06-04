#pragma once

#include <QCloseEvent>
#include <QDialog>
#include <QListWidget>
#include <QWidget>
#include <unordered_map>

#include "camera_record_widget.h"
#include "dir_name_combobox.h"
#include "save_paths_combobox.h"
#include "xdaqvc/camera.h"

class RecordSettings : public QDialog
{
    Q_OBJECT

public:
    explicit RecordSettings(QWidget *parent = nullptr);
    ~RecordSettings() = default;
    void add_camera(Camera *camera);
    void remove_camera(int const id);

    std::unordered_map<int, CameraRecordWidget *> _camera_widget_map;

private:
    QListWidget *_camera_list;
    std::unordered_map<int, QListWidgetItem *> _camera_item_map;

    DirNameComboBox *_dir_name;
    SavePathsComboBox *_save_paths;

protected:
    void closeEvent(QCloseEvent *e) override;
};