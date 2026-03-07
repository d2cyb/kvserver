#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/throw_exception.hpp>
#include <filesystem>
#include <latch>
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

TEST_CASE("server requests benchmark", "[server][benchmark]")
{
	const std::string testFilePath = "bench_config.txt";
	std::filesystem::remove(testFilePath);

	std::latch serverReady { 1 };

	const uint16_t portNum { 8080 };
	std::jthread serverThread([portNum, &testFilePath, &serverReady]() -> void {
		auto config = std::make_shared<Config>(testFilePath);
		config->set("key1", "value 2");

		server = std::make_shared<Server>(portNum, config);

		auto startServerCallBack = [&serverReady]() -> void { serverReady.count_down(); };
		server->pushTask(startServerCallBack);

		server->start();
	});

	serverReady.wait();

	BENCHMARK("request: get")
	{
		sendCommand("localhost", portNum, "get key1");
	};

	BENCHMARK("request: set")
	{
		sendCommand("localhost", portNum, "set key2 = test value 2");
	};

	BENCHMARK("request: wrong command")
	{
		sendCommand("localhost", portNum, "wrong command");
	};

	BENCHMARK("parallel requests: get, set, wrong command")
	{
		std::jthread getThread([portNum]() -> void {
			sendCommand("localhost", portNum, "get key1");
		});
		std::jthread setThread([portNum]() -> void {
			sendCommand("localhost", portNum, "set key2 = test value 2");
		});
		std::jthread undefinedCommandThread([portNum]() -> void {
			sendCommand("localhost", portNum, "wrong command");
		});
	};

	server->stop();

	std::filesystem::remove(testFilePath);
}
