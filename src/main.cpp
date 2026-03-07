#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>

import kvserver.server;
import kvserver.config;

using kvserver::Config;
using kvserver::Server;

std::shared_ptr<Config> config;
std::shared_ptr<Server> server;

void printUsageMessage(const std::string_view &appName)
{
	std::cout << "Usage: " << appName << " <port> <config file>\n"
			  << "Default port: 8080\n"
			  << "Default config: config.txt" << std::endl;
}

void parseArguments() { }

auto main(int argc, char *argv[]) -> int
{
	std::string configFilePath = "config.txt";
	int port { 8080 };
	std::string_view appName { argv[0] };

	if (argc > 1) {
		try {
			port = std::stoi(argv[1]);
			if (port > 65534 || port < 1) {
				throw std::out_of_range("Port value out of range");
			}
			if (argc > 2) {
				configFilePath = argv[2];
			}
		} catch (const std::invalid_argument &err) {
			std::cerr << "Invalid argument" << std::endl;
			printUsageMessage(appName);
			return 1;
		} catch (const std::out_of_range &err) {
			std::cerr << "Argument value out of range" << std::endl;
			printUsageMessage(appName);
			return 1;
		} catch (const std::exception &err) {
			std::cerr << err.what() << std::endl;
			printUsageMessage(appName);
			return 1;
		}
	}

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