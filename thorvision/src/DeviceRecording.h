#pragma once
#include <QObject>
#include <functional>
#include <QTimer>
#include <QVariantMap>
#include <QFutureWatcher>
#include <QPointer>
#include "xdaqvc/recording.h"
#include "CameraModel.h"
#include "Recorder.h"

class DeviceRecording : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    DeviceRecording(CameraModel&,Recorder&,QObject* parent=nullptr);
    QVariantMap state() const {return state_;}
    bool connected() const {return connected_;}
    bool busy() const {return operation_busy_ || state_.value("busy").toBool();}
    QString error() const {return operation_error_.isEmpty()?error_:operation_error_;}
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void storage(const QString& operation,bool confirmed=false);
    Q_INVOKABLE void set_enabled(const QString& identity,bool enabled);
    Q_INVOKABLE void set_segment_seconds(int seconds);
    Q_INVOKABLE void apply_selected_source(bool confirmed);
    Q_INVOKABLE QString status_for(const QString& identity) const;
    void shutdown() { timer_.stop(); pending_operation_={}; disconnect(&watcher_,nullptr,this,nullptr); }
signals:
    void changed();
private:
    CameraModel& cameras_;
    Recorder& recorder_;
    xvc::RecordingClient client_;
    QTimer timer_;
    QFutureWatcher<xvc::RecordingReply> watcher_;
    QVariantMap state_;
    bool connected_=false,request_busy_=false,operation_busy_=false,force_adopt_=false;
    std::function<void()> pending_operation_;
    QString error_;
    QString operation_error_;
    bool operation_request_=false;
    qint64 last_success_=0;
    qint64 resume_deadline_=0;
    QPointer<CameraItem> resume_camera_;
    bool defer_operation(std::function<void()> work);
    void accept(const xvc::RecordingReply&);
    void request(std::function<xvc::RecordingReply()>,bool operation=false);
    nlohmann::json configuration() const;
    void apply(nlohmann::json value);
};
