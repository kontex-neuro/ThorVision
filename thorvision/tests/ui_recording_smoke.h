#pragma once
// Explicit development build only; requires the synthetic API/SRT fixture.
#include <QElapsedTimer>
#include <QDir>
#include <QTimer>
#include <QCoreApplication>
#include <spdlog/spdlog.h>

inline void install_recording_smoke(QCoreApplication& app, CameraModel& cameras,
                                    Recorder& recorder, DeviceRecording& device) {
    auto output=qEnvironmentVariable("THORVISION_PC_SMOKE_DIR");
    if(output.isEmpty())return;
    auto timer=new QTimer(&app);timer->setInterval(250);
    auto elapsed=std::make_shared<QElapsedTimer>();elapsed->start();
    auto phase=std::make_shared<int>(0);
    auto changed_at=std::make_shared<qint64>(0);
    QObject::connect(timer,&QTimer::timeout,&app,[&,timer,elapsed,phase,changed_at,output]{
        if(elapsed->elapsed()>75000) {
            spdlog::error("UI recording smoke timed out in phase {}: {}",*phase,device.error().toStdString());
            timer->stop();app.exit(3);return;
        }
        if(cameras.rowCount()!=4 || !device.connected())return;
        auto camera=cameras.get(0).value("camera_item").value<CameraItem*>();
        if(!camera)return;
        if(*phase==0 && cameras.all_cameras_streaming() && !device.busy()) {
            recorder.settings.set_save_paths({output});
            recorder.settings.set_dir_date(false);recorder.settings.set_dir_name("session");
            if(!recorder.start()){timer->stop();app.exit(4);return;}
            *changed_at=elapsed->elapsed();*phase=1;
        } else if(*phase==1 && elapsed->elapsed()-*changed_at>4000 && !device.busy()) {
            cameras.set_selected_camera_index(0);camera->set_cap("320x240 @ 15FPS");
            device.apply_selected_source(true);*changed_at=elapsed->elapsed();*phase=2;
        } else if(*phase==2 && elapsed->elapsed()-*changed_at>8000 && !device.busy()
                  && !camera->pending_source() && camera->cap()=="320x240 @ 15FPS"
                  && camera->pc_recording_state()=="Recording") {
            recorder.stop();*changed_at=elapsed->elapsed();*phase=3;
        } else if(*phase==3 && elapsed->elapsed()-*changed_at>2000) {
            for(int i=0;i<cameras.rowCount();++i)
                if(!cameras.get(i).value("camera_item").value<CameraItem*>()->recording_finalized())return;
            spdlog::info("UI recording smoke passed: four PC recordings; selected source changed and resumed");
            timer->stop();app.exit(0);
        }
    });
    timer->start();
}
