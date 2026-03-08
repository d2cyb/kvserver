#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <tuple>

import kvserver.server;
import kvserver.config;

using kvserver::Config;
using kvserver::Server;
namespace fs = std::filesystem;

std::shared_ptr<Config> config;
std::shared_ptr<Server> server;

void printUsageMessage(const std::string_view &appName)
{
	std::cout << "Usage: " << appName << " <port> <config file>\n"
			  << "Default port: 8080\n"
			  << "Default config: config.txt" << std::endl;
}

auto parseArguments(int argc, char *argv[]) -> std::tuple<int, std::string>
{
	std::string configFilePath = "config.txt";
	int port { 8080 };

	if (argc > 1) {
		try {
			port = std::stoi(argv[1]);
			if (port > 65534 || port < 1) {
				throw std::out_of_range("Port value out of range");
			}
			if (argc > 2) {
				configFilePath = argv[2];
				if (fs::exists(configFilePath) && not fs::is_regular_file(configFilePath)) {
					throw std::runtime_error("Config is not regular file");
				}
			}
		} catch (const std::invalid_argument &err) {
			throw std::runtime_error(std::string { "Invalid argument: " } + err.what());
		} catch (const std::out_of_range &err) {
			throw std::runtime_error(std::string { "Argument value out of range: " } + err.what());
		} catch (const std::exception &err) {
			throw std::runtime_error(std::string { "Error prase arguments: " } + err.what());
		}
	}
	return { port, configFilePath };
}

auto main(int argc, char *argv[]) -> int
{
	std::string_view appName { argv[0] };

	std::tuple<int, std::string> arguments;
	try {
		arguments = parseArguments(argc, argv);
	} catch (const std::exception &err) {
		printUsageMessage(appName);
		std::cerr << err.what() << std::endl;
		return 1;
	}
	auto [port, configFilePath] = arguments;

	std::cout << "Started key value server\n"
			  << "Server port: " << port << "\n"
			  << "Config file: " << configFilePath << '\n';

	try {
		config = std::make_shared<Config>(configFilePath);
		server = std::make_shared<Server>(port, config);

		server->start();
	} catch (const std::exception &e) {
		std::cerr << "Start server exception: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}