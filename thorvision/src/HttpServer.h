#pragma once

#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include <atomic>
#include <memory>
#include <thread>

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

public:
    Controller(const std::shared_ptr<ObjectMapper> &mapper, Recorder *recorder)
        : oatpp::web::server::api::ApiController(mapper), _recorder(recorder)
    {
    }

    static std::shared_ptr<Controller> createShared(Recorder *recorder = nullptr)
    {
        OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper);
        return std::make_shared<Controller>(objectMapper, recorder);
    }

    ENDPOINT("GET", "/start", start)
    {
        if (!_recorder) {
            return createResponse(
                Status::CODE_500, R"({"status":"error","message":"Recorder not available"})"
            );
        }

        bool started = false;
        QMetaObject::invokeMethod(
            _recorder, "start", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, started)
        );

        return started
                   ? createResponse(
                         Status::CODE_200,
                         R"({"status":"Started","message":"Recording started successfully"})"
                     )
                   : createResponse(
                         Status::CODE_400,
                         R"({"status":"Already Recording","message":"Recorder is already recording"})"
                     );
    }

    ENDPOINT("GET", "/stop", stop)
    {
        if (!_recorder) {
            return createResponse(
                Status::CODE_500, R"({"status":"error","message":"Recorder not available"})"
            );
        }

        bool stopped = false;
        QMetaObject::invokeMethod(
            _recorder, "stop", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, stopped)
        );

        return stopped ? createResponse(
                             Status::CODE_200,
                             R"({"status":"Stopped","message":"Recording stopped successfully"})"
                         )
                       : createResponse(
                             Status::CODE_400,
                             R"({"status":"Not Recording","message":"Recorder was not recording"})"
                         );
    }

    ENDPOINT("GET", "/status", status)
    {
        if (_recorder) {
            auto recording = _recorder->recording();
            auto time = _recorder->recording_time();

            auto response = QString(R"({"status":"%1","recording_time":"%2"})")
                                .arg(recording ? "recording" : "stopped")
                                .arg(time);

            return createResponse(Status::CODE_200, response.toStdString());
        }
        return createResponse(
            Status::CODE_500, R"({"status":"error","message":"Recorder not available"})"
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
        // OATPP_COMPONENT(
        //     std::shared_ptr<oatpp::data::mapping::ObjectMapper>, mapper
        // );  // get ObjectMapper component

        auto handler = oatpp::web::server::HttpConnectionHandler::createShared(router);
        // connectionHandler->setErrorHandler(std::make_shared<ErrorHandler>(mapper));
        return handler;
    }());
};

class HttpServer
{
public:
    HttpServer(Recorder *recorder) : _running(false), _recorder(recorder) { start(); };

    void start()
    {
        if (_running) return;

        _running = true;
        _thread = std::jthread([this]() {
            oatpp::base::Environment::init();
            {
                AppComponent components;  // Create scope Environment components

                OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);

                router->addController(Controller::createShared(_recorder));

                OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, handler);

                OATPP_COMPONENT(
                    std::shared_ptr<oatpp::network::ServerConnectionProvider>, provider
                );

                // oatpp::network::Server server(provider, handler);
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
        if (_server && _running) {
            OATPP_LOGI("ThorVision", "Stopping server...");

            _server->stop();
            _running = false;
            if (_thread.joinable()) {
                _thread.join();
            }

            OATPP_LOGI("ThorVision", "Server stopped.");
        }
    }

private:
    std::jthread _thread;
    std::shared_ptr<oatpp::network::Server> _server;
    std::atomic_bool _running;
    Recorder *_recorder;
};

#endif