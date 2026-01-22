#pragma once

#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <atomic>
#include <memory>
#include <mutex>
#include <stop_token>
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

struct Heartbeat {
    using clock = std::chrono::steady_clock;

    std::atomic<clock::duration::rep> _last_ping;
    std::string _controller_name;
    mutable std::mutex _mtx;

    Heartbeat() : _last_ping(0) {}

    void ping()
    {
        _last_ping.store(clock::now().time_since_epoch().count(), std::memory_order_release);
    }

    bool alive(std::chrono::milliseconds timeout) const
    {
        const auto last = _last_ping.load(std::memory_order_acquire);
        if (last == 0) return false;

        const auto now = clock::now().time_since_epoch().count();
        return clock::duration(now - last) < timeout;
    }

    std::string controller_name() const
    {
        std::lock_guard<std::mutex> lock(_mtx);
        return _controller_name;
    }

    void set_controller_name(std::string_view name)
    {
        std::lock_guard<std::mutex> lock(_mtx);
        _controller_name = name;
    }
};

#include OATPP_CODEGEN_BEGIN(ApiController)

class Controller : public oatpp::web::server::api::ApiController
{
private:
    Recorder *_recorder;
    CameraModel *_camera_model;
    Heartbeat *_heartbeat;

public:
    Controller(
        const std::shared_ptr<ObjectMapper> &mapper, Recorder *recorder, CameraModel *camera_model,
        Heartbeat *heartbeat
    )
        : oatpp::web::server::api::ApiController(mapper),
          _recorder(recorder),
          _camera_model(camera_model),
          _heartbeat(heartbeat)
    {
    }

    static std::shared_ptr<Controller> createShared(
        Recorder *recorder, CameraModel *camera_model, Heartbeat *heartbeat
    )
    {
        if (!recorder || !camera_model || !heartbeat) {
            throw std::logic_error("Controller dependencies must not be null");
        }
        OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper);
        return std::make_shared<Controller>(objectMapper, recorder, camera_model, heartbeat);
    }

    ENDPOINT("PUT", "/start", start, BODY_STRING(String, name))
    {
        if (!name) {
            return createResponse(Status::CODE_400, R"({"status":"Error"})");
        }
        if (!_recorder->api_control()) {
            return createResponse(
                Status::CODE_403, R"({"status":"Error","message":"API control is not enabled"})"
            );
        }
        if (_recorder->recording()) {
            return createResponse(
                Status::CODE_400, R"({"status":"Error","message":"Recorder is already recording"})"
            );
        }
        if (!_camera_model->all_cameras_streaming()) {
            return createResponse(
                Status::CODE_400, R"({"status":"Error","message":"Not all cameras are streaming"})"
            );
        }

        spdlog::info("Recorder Started by {}", name->c_str());
        QMetaObject::invokeMethod(_recorder, "start", Qt::QueuedConnection);
        return createResponse(
            Status::CODE_200, R"({"status":"Success","message":"Recording started successfully"})"
        );
    }

    ENDPOINT("PUT", "/stop", stop, BODY_STRING(String, name))
    {
        if (!name) {
            return createResponse(Status::CODE_400, R"({"status":"Error"})");
        }
        if (!_recorder->api_control()) {
            return createResponse(
                Status::CODE_403, R"({"status":"Error","message":"API control is not enabled"})"
            );
        }
        if (!_recorder->recording()) {
            return createResponse(
                Status::CODE_400, R"({"status":"Error","message":"Recorder is not recording"})"
            );
        }

        spdlog::info("Recorder Stopped by {}", name->c_str());
        QMetaObject::invokeMethod(_recorder, "stop", Qt::QueuedConnection);
        return createResponse(
            Status::CODE_200, R"({"status":"Success","message":"Recording stopped successfully"})"
        );
    }

    ENDPOINT("GET", "/status", status)
    {
        const auto recording = _recorder->recording();
        const auto time = _recorder->recording_time();
        const auto response = QString(R"({"status":"%1","recording_time":"%2"})")
                                  .arg(recording ? "Recording" : "Stopped")
                                  .arg(time)
                                  .toStdString();
        return createResponse(Status::CODE_200, response);
    }

    ENDPOINT("PUT", "/ping", ping, BODY_STRING(String, name))
    {
        if (!name || name->empty()) {
            return createResponse(
                Status::CODE_400, R"({"status":"Error","message":"Invalid request body"})"
            );
        }

        _heartbeat->ping();
        _heartbeat->set_controller_name(name->c_str());
        return createResponse(Status::CODE_200, R"({"status":"alive"})");
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
        : _recorder(recorder),
          _camera_model(camera_model),
          _heartbeat(std::make_unique<Heartbeat>())
    {
        assert(_recorder && "Recorder is null");
        assert(_camera_model && "CameraModel is null");
        start();
    };
    ~HttpServer() { stop(); };

    HttpServer(const HttpServer &) = delete;
    HttpServer &operator=(const HttpServer &) = delete;
    HttpServer(HttpServer &&) = delete;
    HttpServer &operator=(HttpServer &&) = delete;

private:
    std::shared_ptr<oatpp::network::Server> _server;
    QPointer<Recorder> _recorder;
    QPointer<CameraModel> _camera_model;

    std::jthread _server_thread;
    std::jthread _heartbeat_thread;
    std::unique_ptr<Heartbeat> _heartbeat;

    void start()
    {
        _server_thread = std::jthread([this](std::stop_token st) {
            oatpp::base::Environment::init();
            {
                AppComponent components;  // Create scope Environment components

                OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);

                router->addController(
                    Controller::createShared(_recorder, _camera_model, _heartbeat.get())
                );

                OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, handler);
                OATPP_COMPONENT(
                    std::shared_ptr<oatpp::network::ServerConnectionProvider>, provider
                );

                _server = std::make_shared<oatpp::network::Server>(provider, handler);

                std::stop_callback on_stop(st, [srv = _server]() noexcept {
                    if (srv) {
                        srv->stop();
                    }
                });

                OATPP_LOGI(
                    "ThorVision",
                    "Server running on port %s",
                    provider->getProperty("port").toString()->c_str()
                );

                _server->run();
            }
            oatpp::base::Environment::destroy();
        });

        _heartbeat_thread = std::jthread([this](std::stop_token st) {
            constexpr auto heartbeat_timeout = std::chrono::milliseconds(2000);
            constexpr auto check_interval = std::chrono::milliseconds(500);
            bool was_alive = false;

            while (!st.stop_requested()) {
                const bool is_alive = _heartbeat->alive(heartbeat_timeout);

                if (is_alive != was_alive) {
                    const auto name = is_alive ? _heartbeat->controller_name() : "";
                    spdlog::info(
                        "Heartbeat state changed: {} (controller: {})",
                        is_alive,
                        name.empty() ? "none" : name
                    );

                    QMetaObject::invokeMethod(
                        _recorder,
                        [is_alive, name, recorder = QPointer(_recorder)] {
                            if (!recorder) {
                                spdlog::warn("Recorder destroyed before heartbeat update");
                                return;
                            }
                            recorder->set_api_control(is_alive);
                            recorder->set_api_controller_name(QString::fromStdString(name));
                        },
                        Qt::QueuedConnection
                    );

                    if (!is_alive) {
                        _heartbeat->set_controller_name("");
                    }
                    was_alive = is_alive;
                }
                std::this_thread::sleep_for(check_interval);
            }
        });
    }

    void stop()
    {
        OATPP_LOGI("ThorVision", "Stopping server...");

        _server_thread.request_stop();
        _heartbeat_thread.request_stop();
    }
};

#endif