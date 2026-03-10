#include <catch2/catch_test_macros.hpp>

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/throw_exception.hpp>
#include <filesystem>
#include <latch>
#include <memory>
#include <string>
#include <thread>

#include "kvserver_lib_export.h"

import kvserver.server;
import kvserver.config;
import kvserver.statistic;
import kvserver.utilities;

using boost::asio::ip::tcp;
using kvserver::Config;
using kvserver::Server;
using kvserver::Statistic;

using std::string;

const int MAXIMUM_MESSAGE_LENGTH = 1024;

auto sendCommand(const string &host, uint16_t port, const string &command) -> string
{
    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));

        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        // request
        boost::asio::write(socket, boost::asio::buffer(command + '\n'));

        // response
        std::array<char, MAXIMUM_MESSAGE_LENGTH> reply {};
        size_t replyLength = boost::asio::read(
            socket, boost::asio::buffer(reply), boost::asio::transfer_at_least(1)
        );
        string data { reply.data(), replyLength };

        socket.close();

        return data;
    } catch (boost::system::system_error &err) {
        if (err.code() == boost::asio::error::eof) {
            return "connection closed";
        } else {
            return "ERROR: " + string(err.what());
        }
    } catch (std::exception &err) {
        return "ERROR: " + string(err.what());
    }
}

TEST_CASE("server", "[server]")
{
    const uint16_t portNum { 8080 };
    const string testFilePath = "server_config.txt";
    std::shared_ptr<Server> server;

    std::filesystem::remove(testFilePath);

    std::latch serverReady { 1 };
    std::jthread serverThread([portNum, &testFilePath, &serverReady, &server]() -> void {
        auto config = std::make_shared<Config>(testFilePath);
        config->set("key1", "value 2");

        auto statistic = std::make_shared<Statistic>();

        server = std::make_shared<Server>(portNum, config, statistic);

        auto startServerCallBack = [&serverReady]() -> void { serverReady.count_down(); };
        server->pushTask(startServerCallBack);

        server->start();
    });

    SECTION("procession commands")
    {
        serverReady.wait();

        auto resultGet = sendCommand("localhost", portNum, "get key1");
        REQUIRE(resultGet == "key1=value 2\nreads=1\nwrites=1");

        auto resultSet = sendCommand("localhost", portNum, "set key2 = test value 2");
        REQUIRE(resultSet == "key2=test value 2\nreads=0\nwrites=1");

        auto resultUndefinedCommand = sendCommand("localhost", portNum, "wrong command");
        REQUIRE(resultUndefinedCommand == "(empty)");

        server->stop();
    }

    SECTION("must receive message less 4049 chars")
    {
        serverReady.wait();

        const int MAX_MESSAGE_SIZE { 4095 };
        string tooLargeMessage(MAX_MESSAGE_SIZE, 'A');
        auto resultGet = sendCommand("localhost", portNum, tooLargeMessage);

        REQUIRE(resultGet == "(empty)");

        server->stop();
    }

    SECTION("must close connection if message too large")
    {
        serverReady.wait();

        const int TOO_LARGE_MESSAGE_SIZE { 4096 };
        string tooLargeMessage(TOO_LARGE_MESSAGE_SIZE, 'A');
        auto resultGet = sendCommand("localhost", portNum, tooLargeMessage);

        REQUIRE(resultGet == "connection closed");

        server->stop();
    }

    std::filesystem::remove(testFilePath);
}

TEST_CASE("server parallel", "[server]")
{
    const uint16_t portNum { 8080 };
    const string testFilePath = "parallel_server_config.txt";
    std::shared_ptr<Server> server;

    std::filesystem::remove(testFilePath);

    std::latch serverReady { 1 };
    std::jthread serverThread([portNum, &testFilePath, &serverReady, &server]() -> void {
        auto config = std::make_shared<Config>(testFilePath);
        config->set("key1", "value 2");

        auto statistic = std::make_shared<Statistic>();
        server         = std::make_shared<Server>(portNum, config, statistic);

        auto startServerCallBack = [&serverReady]() -> void { serverReady.count_down(); };
        server->pushTask(startServerCallBack);

        server->start();
    });

    SECTION("procession commands")
    {
        serverReady.wait();

        int requestCount { 3 };
        std::latch allRequestsDone(requestCount);

        auto getKeyFuture               = std::async([portNum, &allRequestsDone]() -> string {
            auto response = sendCommand("localhost", portNum, "get key1");
            allRequestsDone.count_down();
            return response;
        });
        auto setAndGetValueFuture       = std::async([portNum, &allRequestsDone]() -> string {
            sendCommand("localhost", portNum, "set key2 = test value 2");
            auto response = sendCommand("localhost", portNum, "get key2");
            allRequestsDone.count_down();
            return response;
        });
        auto sendUndefinedCommandFuture = std::async([portNum, &allRequestsDone]() -> string {
            auto response = sendCommand("localhost", portNum, "wrong command");
            allRequestsDone.count_down();
            return response;
        });

        string getKeyResponse           = getKeyFuture.get();
        string setAndGetValueResponse   = setAndGetValueFuture.get();
        string undefinedCommandResponse = sendUndefinedCommandFuture.get();

        allRequestsDone.wait();

        REQUIRE(getKeyResponse == "key1=value 2\nreads=1\nwrites=1");
        REQUIRE(setAndGetValueResponse == "key2=test value 2\nreads=1\nwrites=1");
        REQUIRE(undefinedCommandResponse == "(empty)");

        server->stop();
    }

    std::filesystem::remove(testFilePath);
}
