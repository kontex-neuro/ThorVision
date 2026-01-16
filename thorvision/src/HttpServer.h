#pragma once

#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <atomic>
#include <memory>
#include <thread>

#include "CameraModel.h"
#include "Recorder.h"
#include "oatpp/core/macro/codegen.hpp"
#include "oatpp/core/macro/component.hpp"
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/api/ApiController.hpp"

#include OATPP_CODEGEN_BEGIN(ApiController)

class Controller : public oatpp::web::server::api::ApiController
{
private:
    Recorder *_recorder;
    CameraModel *_camera_model;

public:
    Controller(
        const std::shared_ptr<ObjectMapper> &mapper, Recorder *recorder, CameraModel *camera_model
    )
        : oatpp::web::server::api::ApiController(mapper),
          _recorder(recorder),
          _camera_model(camera_model)
    {
    }

    static std::shared_ptr<Controller> createShared(
        Recorder *recorder = nullptr, CameraModel *camera_model = nullptr
    )
    {
        OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper);
        return std::make_shared<Controller>(objectMapper, recorder, camera_model);
    }

    ENDPOINT("PUT", "/start", start, BODY_STRING(String, name))
    {
        if (!_recorder) {
            return createResponse(
                Status::CODE_500, R"({"status":"Error","message":"Recorder not available"})"
            );
        }

        if (_recorder->recording()) {
            return createResponse(
                Status::CODE_400, R"({"status":"Error","message":"Recorder is already recording"})"
            );
        }

        if (!name) {
            return createResponse(
                Status::CODE_400, R"({"status":"Error","message":"Invalid request body"})"
            );
        }

        if (!_camera_model->all_cameras_streaming()) {
            spdlog::info("not all cameras streaming, cannot start recorder");
            return createResponse(
                Status::CODE_400, R"({"status":"Error","message":"Not all cameras are streaming"})"
            );
        }

        bool started = false;
        QMetaObject::invokeMethod(
            _recorder, "start", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, started)
        );
        QMetaObject::invokeMethod(
            _recorder, "set_api_control", Qt::QueuedConnection, Q_ARG(bool, true)
        );
        QMetaObject::invokeMethod(
            _recorder,
            "set_api_controller_name",
            Qt::QueuedConnection,
            Q_ARG(QString, QString::fromStdString(*name))
        );

        auto process_name = name->c_str();

        OATPP_LOGI("Recorder", "Started by %s", process_name);

        return started ? createResponse(
                             Status::CODE_200,
                             R"({"status":"Success","message":"Recording started successfully"})"
                         )
                       : createResponse(
                             Status::CODE_400,
                             R"({"status":"Error","message":"Recorder is already
                         recording"})"
                         );
    }

    ENDPOINT("PUT", "/stop", stop, BODY_STRING(String, name))
    {
        if (!_recorder) {
            return createResponse(
                Status::CODE_500, R"({"status":"Error","message":"Recorder not available"})"
            );
        }

        bool stopped = false;
        QMetaObject::invokeMethod(
            _recorder, "stop", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, stopped)
        );
        QMetaObject::invokeMethod(
            _recorder, "set_api_control", Qt::QueuedConnection, Q_ARG(bool, false)
        );
        QMetaObject::invokeMethod(
            _recorder, "set_api_controller_name", Qt::QueuedConnection, Q_ARG(QString, QString())
        );

        auto process_name = name->c_str();
        OATPP_LOGI("Recorder", "Stopped by %s", process_name);

        return stopped ? createResponse(
                             Status::CODE_200,
                             R"({"status":"Success","message":"Recording stopped successfully"})"
                         )
                       : createResponse(
                             Status::CODE_400,
                             R"({"status":"Error","message":"Recorder was not recording"})"
                         );
    }

    ENDPOINT("GET", "/status", status)
    {
        if (_recorder) {
            const auto recording = _recorder->recording();
            const auto &time = _recorder->recording_time();

            const auto &response = QString(R"({"status":"%1","recording_time":"%2"})")
                                       .arg(recording ? "Recording" : "Stopped")
                                       .arg(time);

            return createResponse(Status::CODE_200, response.toStdString());
        }
        return createResponse(
            Status::CODE_500, R"({"status":"Error","message":"Recorder not available"})"
        );
    }
};

#include OATPP_CODEGEN_END(ApiController)

/**
 *  Class which creates and holds Application components and registers components in
 * oatpp::base::Environment Order of components initialization is from top to bottom
 */
class AppComponent
{
public:
    /**
     * Create ObjectMapper component to serialize/deserialize DTOs in Controller's API
     */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, mapper)([] {
        auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
        objectMapper->getDeserializer()->getConfig()->allowUnknownFields = false;
        return objectMapper;
    }());

    /**
     *  Create ConnectionProvider component which listens on the port
     */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, provider)([] {
        return oatpp::network::tcp::server::ConnectionProvider::createShared(
            {"localhost", 8001, oatpp::network::Address::IP_4}
        );
    }());

    /**
     *  Create Router component
     */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router)([] {
        return oatpp::web::server::HttpRouter::createShared();
    }());

    /**
     *  Create ConnectionHandler component which uses Router component to route requests
     */
    OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, handler)([] {
        OATPP_COMPONENT(
            std::shared_ptr<oatpp::web::server::HttpRouter>, router
        );  // get Router component

        auto handler = oatpp::web::server::HttpConnectionHandler::createShared(router);
        return handler;
    }());
};

class HttpServer
{
public:
    HttpServer(Recorder *recorder, CameraModel *camera_model)
        : _running(false), _recorder(recorder), _camera_model(camera_model)
    {
        start();
    };
    ~HttpServer() { stop(); };

    void start()
    {
        if (_running) return;

        _running = true;
        _thread = std::jthread([this]() {
            oatpp::base::Environment::init();
            {
                AppComponent components;  // Create scope Environment components

                OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);

                router->addController(Controller::createShared(_recorder, _camera_model));

                OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, handler);

                OATPP_COMPONENT(
                    std::shared_ptr<oatpp::network::ServerConnectionProvider>, provider
                );

                _server = std::make_shared<oatpp::network::Server>(provider, handler);

                OATPP_LOGI(
                    "ThorVision",
                    "Server running on port %s",
                    provider->getProperty("port").toString()->c_str()
                );

                _server->run();

                _running = false;
            }
            oatpp::base::Environment::destroy();
        });
    }

    void stop()
    {
        if (!_running) return;

        OATPP_LOGI("ThorVision", "Stopping server...");

        if (_server) {
            _server->stop();
        }
        if (_thread.joinable()) {
            _thread.join();
        }
        _running = false;

        OATPP_LOGI("ThorVision", "Server stopped.");
    }

private:
    std::jthread _thread;
    std::shared_ptr<oatpp::network::Server> _server;
    std::atomic_bool _running;
    Recorder *_recorder;
    CameraModel *_camera_model;
};

#endif