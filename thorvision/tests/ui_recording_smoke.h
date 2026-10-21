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

inline void install_refresh_soak(QCoreApplication& app,QQuickWindow* window,DeviceRecording& device) {
    bool ok=false;auto duration=qEnvironmentVariableIntValue("THORVISION_UI_SOAK_MS",&ok);
    if(!ok || duration<1000)return;
    auto timer=new QTimer(&app);timer->setInterval(1000);
    auto elapsed=std::make_shared<QElapsedTimer>();elapsed->start();
    auto first=std::make_shared<QPointer<QObject>>();
    auto replacements=std::make_shared<int>(0);
    auto toggles=std::make_shared<int>(0);auto was_busy=std::make_shared<bool>(device.busy());
    QObject::connect(&device,&DeviceRecording::changed,&app,[&,was_busy,toggles]{
        if(device.busy()!=*was_busy){++*toggles;*was_busy=device.busy();}
    });
    if(qEnvironmentVariableIsSet("THORVISION_UI_QUEUE_SMOKE"))QTimer::singleShot(5000,&app,[&device]{
        device.refresh(); // Deliberately overlap a background read and a user mutation.
        device.set_segment_seconds(61);
    });
    QObject::connect(timer,&QTimer::timeout,&app,[&,window,timer,elapsed,first,replacements,toggles,duration]{
        QList<QObject*> rows;
        std::function<void(QQuickItem*)> visit=[&](QQuickItem* item){
            if(item->objectName()=="deviceRecordingCameraRow")rows.append(item);
            for(auto child:item->childItems())visit(child);
        };visit(window->contentItem());
        if(!rows.empty()) {
            if(first->isNull()){if(*replacements || elapsed->elapsed()>4000)++*replacements;*first=rows.front();}
            else if(first->data()!=rows.front()){++*replacements;*first=rows.front();}
        }
        spdlog::info("REFRESH_SOAK elapsed={} rows={} objects={} replacements={} busy_edges={}",
            elapsed->elapsed(),rows.size(),window->findChildren<QObject*>().size(),*replacements,*toggles);
        if(elapsed->elapsed()>=duration){timer->stop();app.exit(0);}
    });timer->start();
}

inline void install_storage_flow_smoke(QCoreApplication& app,QQuickWindow* window,DeviceRecording& device) {
    auto output=qEnvironmentVariable("THORVISION_STORAGE_FLOW_DIR");
    if(output.isEmpty())return;
    auto timer=new QTimer(&app);timer->setInterval(100);
    auto clock=std::make_shared<QElapsedTimer>();clock->start();
    auto phase=std::make_shared<int>(0);auto since=std::make_shared<qint64>(0);
    QObject::connect(timer,&QTimer::timeout,&app,[&,window,timer,clock,phase,since,output]{
        auto fail=[&](const char* message){spdlog::error("Storage flow smoke: {}",message);timer->stop();app.exit(4);};
        auto object=[&](const char* name){return window->findChild<QObject*>(name);};
        auto click=[&](const char* name){auto item=object(name);return item && QMetaObject::invokeMethod(item,"clicked");};
        auto visible=[&](const char* name){auto item=object(name);return item && item->property("visible").toBool();};
        auto next=[&]{++*phase;*since=clock->elapsed();};
        if(clock->elapsed()>20000){fail("timed out");return;}
        if(!device.connected())return;
        if(*phase==0) {
            if(!click("ssdDetailsButton") || !visible("ssdDetailsDialog")){fail("details did not open");return;}
            next();
        } else if(*phase==1 && clock->elapsed()-*since>400) {
            window->grabWindow().save(output+"/ssd-details.png");
            if(device.busy()){fail("opening details changed device state");return;}
            QMetaObject::invokeMethod(object("ssdDetailsDialog"),"reject");next();
        } else if(*phase==2 && clock->elapsed()-*since>1300) {
            if(visible("ssdDetailsDialog")){fail("details did not close");return;}
            if(device.busy() || device.state().value("test_operation_count").toInt()!=0){fail("viewing details triggered a storage operation");return;}
            spdlog::info("Storage flow smoke passed: opening and closing SSD details is read-only");
            timer->stop();app.exit(0);
        }
    });timer->start();
}
