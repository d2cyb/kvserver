#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "kvserver_lib_export.h"

import kvserver.statistic;

using kvserver::Statistic;
using std::string;

const uint32_t STATISTIC_OUTOUT_INTERVAL = 100;

class OutputStreamRedirector {
private:
	std::streambuf *originalBuffer;
	std::stringstream outputBuffer;
	std::ostream &stream;

public:
	explicit OutputStreamRedirector(std::ostream &stream)
		: originalBuffer(stream.rdbuf())
		, stream(stream)
	{
		stream.rdbuf(outputBuffer.rdbuf());
	}
	OutputStreamRedirector(const OutputStreamRedirector &)						= delete;
	OutputStreamRedirector(const OutputStreamRedirector &&)						= delete;
	auto operator=(const OutputStreamRedirector &) -> OutputStreamRedirector &	= delete;
	auto operator=(const OutputStreamRedirector &&) -> OutputStreamRedirector & = delete;

	~OutputStreamRedirector()
	{
		stream.rdbuf(originalBuffer);
	}

	auto getOutput() const -> std::string
	{
		return outputBuffer.str();
	}
};

TEST_CASE("Statistic", "[statistic]")
{
	SECTION("should stats empty on init")
	{
		Statistic stats { STATISTIC_OUTOUT_INTERVAL };
		REQUIRE(stats.getTotalCommands() == 0);
		REQUIRE(stats.getRecentCommands() == 0);
	}

	SECTION("should calculate count of commands")
	{
		Statistic stats { STATISTIC_OUTOUT_INTERVAL };
		stats.recordCommand();
		REQUIRE(stats.getTotalCommands() == 1);
		REQUIRE(stats.getRecentCommands() == 1);

		stats.recordCommand();
		stats.recordCommand();
		REQUIRE(stats.getTotalCommands() == 3);
		REQUIRE(stats.getRecentCommands() == 3);
	}

	SECTION("should reset recent commands count, after time interval")
	{
		Statistic stats { STATISTIC_OUTOUT_INTERVAL };

		for (int i = 0; i < 3; ++i) {
			stats.recordCommand();
		}

		// calculation before time interval
		REQUIRE(stats.getTotalCommands() == 3);
		REQUIRE(stats.getRecentCommands() == 3);

		std::this_thread::sleep_for(std::chrono::milliseconds(STATISTIC_OUTOUT_INTERVAL + 1));

		for (int i = 0; i < 5; ++i) {
			stats.recordCommand();
		}
		// calculation after time interval
		REQUIRE(stats.getTotalCommands() == 8);
		REQUIRE(stats.getRecentCommands() == 5);

		std::this_thread::sleep_for(std::chrono::milliseconds(STATISTIC_OUTOUT_INTERVAL + 1));

		// calculation after second interval, without requests
		REQUIRE(stats.getTotalCommands() == 8);
		REQUIRE(stats.getRecentCommands() == 0);
	}

	SECTION("should calculate count of concurrent commands")
	{
		Statistic stats { STATISTIC_OUTOUT_INTERVAL };
		const int threadsNum	   = 10;
		const int comandsPerThread = 100;

		std::vector<std::thread> threads;
		threads.reserve(threadsNum);

		for (int t = 0; t < threadsNum; ++t) {
			threads.emplace_back([&stats, comandsPerThread]() -> void {
				for (int i = 0; i < comandsPerThread; ++i) {
					stats.recordCommand();
				}
			});
		}

		for (auto &thread : threads) {
			thread.join();
		}

		REQUIRE(stats.getTotalCommands() == threadsNum * comandsPerThread);
	}

	SECTION("should output statistics first interval, after time interval")
	{
		Statistic stats { STATISTIC_OUTOUT_INTERVAL };
		OutputStreamRedirector redirector { std::cout };

		for (int i = 0; i < 3; ++i) {
			stats.recordCommand();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(STATISTIC_OUTOUT_INTERVAL + 1));

		for (int i = 0; i < 5; ++i) {
			stats.recordCommand();
		}

		auto output = redirector.getOutput();
		REQUIRE(output.find("Total commands : 3") != string::npos);
		REQUIRE(output.find("Commands in last 0.1 seconds: 3") != string::npos);
	}
}
