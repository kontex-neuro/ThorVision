#include "DeviceRecording.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QSettings>
#include <memory>

using nlohmann::json;
DeviceRecording::DeviceRecording(CameraModel& cameras,Recorder& recorder,QObject* parent)
    :QObject(parent),cameras_(cameras),recorder_(recorder) {
    dismissed_job_=QSettings().value("device_recording/dismissed_repair_job").toString();
    connect(&watcher_,&QFutureWatcher<xvc::RecordingReply>::finished,this,[this]{
        request_busy_=false;
        if(operation_request_)operation_busy_=false;
        if(pending_operation_)QTimer::singleShot(0,this,[this]{
            if(request_busy_ || !pending_operation_)return;
            auto pending=std::move(pending_operation_);pending_operation_={};
            operation_busy_=false;pending();emit changed();
        });
        auto reply=watcher_.result();
        accept(reply);
        if(resume_camera_ && !reply) {
            if(QDateTime::currentMSecsSinceEpoch()<resume_deadline_) {
                force_adopt_=true;emit changed();return;
            }
            resume_camera_=nullptr;
            error_=tr("Source state could not be refreshed; check the camera before restarting PC REC.");
        }
        if(resume_camera_) {
            auto camera=resume_camera_;resume_camera_=nullptr;
            auto retry=std::make_shared<int>(0);
            auto timer=new QTimer(this);timer->setInterval(250);
            connect(timer,&QTimer::timeout,this,[this,camera,retry,timer]{
                if(!camera || !recorder_.recording() || ++*retry>40) {
                    if(camera && *retry>40)error_=tr("PC recording could not resume; check the camera and restart PC REC.");
                    timer->stop();timer->deleteLater();emit changed();return;
                }
                if(camera->streaming() && camera->recording_finalized()) {
                    if(!camera->start_recording(recorder_.settings))error_=tr("PC recording could not resume; check the save location.");
                    timer->stop();timer->deleteLater();emit changed();
                }
            });timer->start();
        }
        emit changed();
    });
    timer_.setInterval(1000);
    connect(&timer_,&QTimer::timeout,this,[this]{
        if(connected_ && last_success_ && QDateTime::currentMSecsSinceEpoch()-last_success_>3500){connected_=false;emit changed();}
        refresh();
    });timer_.start();refresh();
}
void DeviceRecording::request(std::function<xvc::RecordingReply()> work,bool operation) {
    if(request_busy_)return;
    operation_request_=operation;
    request_busy_=true;
    if(operation){operation_busy_=true;emit changed();}
    watcher_.setFuture(QtConcurrent::run(std::move(work)));
}
bool DeviceRecording::defer_operation(std::function<void()> work) {
    if(!request_busy_)return false;
    pending_operation_=std::move(work);operation_busy_=true;emit changed();
    return true;
}
void DeviceRecording::refresh() {
    if(request_busy_ || operation_busy_)return;
    auto client=client_;request([client]{return client.state();});
}
void DeviceRecording::accept(const xvc::RecordingReply& reply) {
    if(!reply) {
        error_=QString::fromStdString(reply.error);
        if(operation_request_)operation_error_=error_;
        if(reply.status==0)connected_=false;
        // On an apply failure restore the actual device snapshot before resuming PC REC.
        return;
    }
    connected_=true;last_success_=QDateTime::currentMSecsSinceEpoch();error_.clear();
    if(operation_request_)operation_error_.clear();
    state_=QJsonDocument::fromJson(QByteArray::fromStdString(reply.state.dump())).object().toVariantMap();
    for(auto value:state_.value("cameras").toList()) {
        auto state=value.toMap();int id=state.value("id",-1).toInt();
        for(int i=0;i<cameras_.rowCount();++i) {
            auto camera=cameras_.get(i).value("camera_item").value<CameraItem*>();
            if(camera && (camera->stable_id()==state.value("stable_id").toString() || (camera->stable_id().isEmpty() && camera->id()==id)))
                camera->adopt_profile(state,force_adopt_);
        }
    }
    force_adopt_=false;
}
json DeviceRecording::configuration() const {
    return {{"revision",state_.value("revision").toInt()},
            {"segment_seconds",state_.value("segment_seconds",60).toInt()},
            {"cameras",json::array()},{"confirmed_source_change",false}};
}
void DeviceRecording::apply(json value) {
    if(!connected_ || busy())return;
    force_adopt_=true;auto client=client_;
    request([client,value]{return client.apply(value);},true);
}
void DeviceRecording::set_enabled(const QString& identity,bool enabled) {
    if(!connected_ || busy())return;
    if(defer_operation([this,identity,enabled]{set_enabled(identity,enabled);}))return;
    auto value=configuration();
    value["cameras"].push_back({{"stable_id",identity.toStdString()},{"enabled",enabled}});
    apply(value);
}
void DeviceRecording::set_segment_seconds(int seconds) {
    if(!connected_ || busy())return;
    if(defer_operation([this,seconds]{set_segment_seconds(seconds);}))return;
    auto value=configuration();value["segment_seconds"]=seconds;apply(value);
}
void DeviceRecording::storage(const QString& operation,bool confirmed) {
    if(!connected_ || busy())return;
    if(defer_operation([this,operation,confirmed]{storage(operation,confirmed);}))return;
    auto storage=state_.value("storage").toMap();
    json token={{"uuid",storage.value("uuid").toString().toStdString()},
                {"generation",storage.value("generation").toString().toStdString()}, {"confirmed",confirmed}};
    auto client=client_;auto name=operation.toStdString();request([client,name,token]{return client.storage(name,token);},true);
}
void DeviceRecording::apply_selected_source(bool confirmed) {
    if(!confirmed || !connected_ || busy())return;
    if(defer_operation([this,confirmed]{apply_selected_source(confirmed);}))return;
    auto camera=cameras_.get(cameras_.selected_camera_index()).value("camera_item").value<CameraItem*>();
    if(!camera)return;
    auto profile=camera->selected_profile();
    if(profile.isEmpty()){error_=tr("Select a supported codec and resolution/FPS combination.");emit changed();return;}
    auto value=configuration();auto identity=camera->stable_id();bool enabled=true;
    for(auto item:state_.value("cameras").toList())if(item.toMap().value("stable_id").toString()==identity)enabled=item.toMap().value("enabled").toBool();
    value["confirmed_source_change"]=true;
    value["cameras"].push_back({{"stable_id",identity.toStdString()},{"enabled",enabled},
        {"profile",json::parse(QJsonDocument::fromVariant(profile).toJson().toStdString())}});
    if(!recorder_.recording()){apply(value);return;}
    if(!camera->stop_recording()){error_=tr("PC recording could not finalize; source settings were not changed.");emit changed();return;}
    auto guard=QPointer<CameraItem>(camera);auto started=QDateTime::currentMSecsSinceEpoch();
    request_busy_=true;operation_busy_=true;emit changed();
    auto timer=new QTimer(this);timer->setInterval(100);
    connect(timer,&QTimer::timeout,this,[this,guard,timer,started,value]{
        if(!guard || QDateTime::currentMSecsSinceEpoch()-started>5000) {
            request_busy_=false;operation_busy_=false;error_=tr("PC recording did not finish in time; source settings were not changed.");
            timer->stop();timer->deleteLater();emit changed();return;
        }
        if(guard->recording_finalized()) {
            request_busy_=false;operation_busy_=false;resume_camera_=guard;resume_deadline_=QDateTime::currentMSecsSinceEpoch()+15000;apply(value);timer->stop();timer->deleteLater();
        }
    });timer->start();
}
void DeviceRecording::set_dismissed_job(const QString& id) {
    if(id==dismissed_job_)return;
    dismissed_job_=id;
    QSettings().setValue("device_recording/dismissed_repair_job",id);
    emit dismissed_job_changed();
}
QString DeviceRecording::status_for(const QString& identity) const {
    if(!connected_)return tr("Unknown");
    for(auto item:state_.value("cameras").toList()) {
        auto camera=item.toMap();if(camera.value("stable_id").toString()==identity)return camera.value("state").toString();
    }
    return tr("Not selected");
}
