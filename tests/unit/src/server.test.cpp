#include <catch2/catch_test_macros.hpp>

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/throw_exception.hpp>
#include <filesystem>
#include <future>
#include <latch>
#include <memory>
#include <string>
#include <thread>

#include "kvserver_lib_export.h"

import kvserver.server;
import kvserver.config;
import kvserver.utilities;

using boost::asio::ip::tcp;
using kvserver::Config;
using kvserver::Server;

const int MAXIMUM_MESSAGE_LENGTH = 1024;

auto sendCommand(const std::string &host, uint16_t port, const std::string &command) -> std::string
{
	try {
		boost::asio::io_context io_context {};
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
		std::string data { reply.data(), replyLength };

		socket.close();

		return data;
	} catch (std::exception &e) {
		return "ERROR: " + std::string(e.what());
	}
}

std::shared_ptr<Server> server;

TEST_CASE("server", "[server]")
{
	const uint16_t portNum { 8080 };
	const std::string testFilePath = "server_config.txt";

	SECTION("procession commands")
	{
		std::filesystem::remove(testFilePath);

		std::latch serverReady { 1 };
		std::jthread serverThread([portNum, &testFilePath, &serverReady]() -> void {
			auto config = std::make_shared<Config>(testFilePath);
			config->set("key1", "value 2");

			server = std::make_shared<Server>(portNum, config);

			auto startServerCallBack = [&serverReady]() -> void { serverReady.count_down(); };
			server->pushTask(startServerCallBack);

			server->start();
		});

		serverReady.wait();

		// should return value on get commands
		auto resultGet = sendCommand("localhost", portNum, "get key1");
		REQUIRE(resultGet == "value 2");

		// should return empty string on set commands
		auto resultSet = sendCommand("localhost", portNum, "set key2 = test value 2");
		REQUIRE(resultSet == "(empty)");

		// should return empty string on wrong commands
		auto resultUndefinedCommand = sendCommand("localhost", portNum, "wrong command");
		REQUIRE(resultUndefinedCommand == "(empty)");

		server->stop();
	}

	std::filesystem::remove(testFilePath);
}

TEST_CASE("server parallel", "[server]")
{
	const uint16_t portNum { 8080 };
	const std::string testFilePath = "parallel_server_config.txt";

	SECTION("procession commands")
	{
		std::filesystem::remove(testFilePath);

		std::latch serverReady { 1 };
		std::jthread serverThread([portNum, &testFilePath, &serverReady]() -> void {
			auto config = std::make_shared<Config>(testFilePath);
			config->set("key1", "value 2");

			server = std::make_shared<Server>(portNum, config);

			auto startServerCallBack = [&serverReady]() -> void { serverReady.count_down(); };
			server->pushTask(startServerCallBack);

			server->start();
		});

		serverReady.wait();

		int requestCount { 3 };
		std::latch allRequestsDone(requestCount);

		auto getKeyFuture				= std::async([portNum, &allRequestsDone]() -> std::string {
			  auto response = sendCommand("localhost", portNum, "get key1");
			  allRequestsDone.count_down();
			  return response;
		  });
		auto setAndGetValueFuture		= std::async([portNum, &allRequestsDone]() -> std::string {
			  sendCommand("localhost", portNum, "set key2 = test value 2");
			  auto response = sendCommand("localhost", portNum, "get key2");
			  allRequestsDone.count_down();
			  return response;
		  });
		auto sendUndefinedCommandFuture = std::async([portNum, &allRequestsDone]() -> std::string {
			auto response = sendCommand("localhost", portNum, "wrong command");
			allRequestsDone.count_down();
			return response;
		});

		std::string getKeyResponse			 = getKeyFuture.get();
		std::string setAndGetValueResponse	 = setAndGetValueFuture.get();
		std::string undefinedCommandResponse = sendUndefinedCommandFuture.get();

		allRequestsDone.wait();

		REQUIRE(getKeyResponse == "value 2");
		REQUIRE(setAndGetValueResponse == "test value 2");
		REQUIRE(undefinedCommandResponse == "(empty)");

		server->stop();
	}

	std::filesystem::remove(testFilePath);
}
